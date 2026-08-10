#include <furi.h>
#include <furi_hal_power.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_box.h>
#include <storage/storage.h>
#include <math.h>

#include "i2c_worker.h"
#include "chip_db.h"
#include "i2c_notify.h"
#include "i2c_settings.h"
#include "report.h"

#define TAG "FakeChipDetector"

#define ANIM_PERIOD_MS 60 // ~16 fps, smooth enough and cheap

typedef enum {
    FakeChipViewMenu,
    FakeChipViewWiring,
    FakeChipViewScan,
    FakeChipViewDetail,
    FakeChipViewLive,
    FakeChipViewSettings,
    FakeChipViewChips,
    FakeChipViewReport,
    FakeChipViewAbout,
} FakeChipViewId;

typedef enum {
    MenuIndexWiring,
    MenuIndexScan,
    MenuIndexLiveTest,
    MenuIndexSettings,
    MenuIndexChips,
    MenuIndexAbout,
} MenuIndex;

// Half-width of the break in each wire, in pixels. Animates to zero as the
// line comes alive, so a connection visibly closes the circuit.
#define WIRE_GAP_MAX 10

typedef struct {
    uint32_t frame;
    I2CBusCheck bus;
    bool sensor_seen; // pull-ups detected => something is wired up
    uint8_t gap[4]; // per-row animated break
} WiringViewModel;

typedef struct {
    bool scanning;
    uint32_t frame;
    uint8_t progress_addr;
    I2CFoundDevice found[I2C_SCAN_MAX_FOUND];
    uint8_t found_count;
    uint8_t selected;
    uint8_t scroll;
    I2CBusCheck bus; // captured before the sweep, drives the failure hints
    char status_msg[20];
    char saved_name[32]; // filename of the last report, shown after saving
    // Knowing which chip it is only answers half the question. The other half
    // is whether that is the chip the user paid for, and only they know that.
    enum {
        AnswerAsking,
        AnswerExpected,
        AnswerNotWhatIOrdered,
    } answer;
} ScanViewModel;

typedef struct {
    I2CFoundDevice device;
} DetailViewModel;

typedef struct {
    I2CLiveData data;
    uint32_t frame;
} LiveViewModel;

typedef struct {
    uint16_t selected;
} ChipsViewModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    View* wiring_view;
    View* scan_view;
    View* detail_view;
    View* live_view;
    View* chips_view;
    TextBox* report_box;
    FuriString* report_text;
    VariableItemList* settings_list;
    Widget* about_widget;
    I2CWorker* worker;
    FuriThread* anim_thread;
    volatile bool anim_stop;
    I2CSettings settings;
    volatile FakeChipViewId current_view;
    uint8_t last_mag_cal; // to chime once when calibration reaches 3
} FakeChipApp;

static void app_switch_view(FakeChipApp* app, FakeChipViewId view_id) {
    app->current_view = view_id;
    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}

/* ---------------- Wiring screen ---------------- */

// Compact plug glyph so the boxes read as hardware, not plain rectangles.
static void draw_connector(Canvas* canvas, uint8_t x, uint8_t y) {
    canvas_draw_box(canvas, x, y - 2, 3, 5);
    canvas_draw_line(canvas, x + 3, y, x + 5, y);
}

// The wire itself carries the state, so there is nothing extra to decode:
// a broken line means not connected, the break closes up when the line comes
// alive, and a cross in the break means a fault.
typedef enum {
    WireMissing,
    WireLive,
    WireFault,
} WireState;

// Row order on screen: GND, 3V3, SDA, SCL.
static WireState wiring_state(const I2CBusCheck* bus, uint8_t row) {
    switch(row) {
    case 0:
    case 1:
        // GND and 3V3 cannot be sensed directly — the Flipper drives them.
        // But a pull-up only reads high if the module's supply is live and
        // shares our ground, so one pulled-up line already proves both.
        return bus->powered ? WireLive : WireMissing;
    case 2:
        return bus->sda_stuck ? WireFault : (bus->sda_ok ? WireLive : WireMissing);
    default:
        return bus->scl_stuck ? WireFault : (bus->scl_ok ? WireLive : WireMissing);
    }
}

// Wire rows: text lives in a gap in the line rather than on top of it, so
// nothing overlaps on a 128x64 screen.
#define WIRE_X0 38 // line starts after the Flipper pin label
#define WIRE_X1 84 // line ends before the sensor signal label

static void wiring_draw_callback(Canvas* canvas, void* model) {
    WiringViewModel* m = model;
    canvas_clear(canvas);

    // Baseline 9, not 7: FontPrimary caps are ~7px tall and would be clipped
    // by the top edge of the screen.
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "3.3V ONLY - NOT 5V!");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 17, "FLIPPER");
    canvas_draw_str(canvas, 88, 17, "SENSOR");
    canvas_draw_line(canvas, 0, 19, 127, 19);

    // Listed in ascending pin order so the numbers read in sequence and each
    // pair sits together on the header: 8 and 9 are neighbours in the top
    // row, 15 and 16 in the bottom row. Power first also matches the order
    // you actually want to wire things up in.
    // Pin numbers verified against furi_hal_resources.c gpio_pins[]:
    // PC0 (SCL) is header pin 16, PC1 (SDA) is header pin 15.
    const char* pins[] = {"pin 8", "pin 9", "pin 15", "pin 16"};
    const char* signals[] = {"GND", "3V3", "SDA", "SCL"};

    const uint8_t mid = (WIRE_X0 + WIRE_X1) / 2;

    for(uint8_t i = 0; i < 4; i++) {
        uint8_t baseline = 28 + i * 8;
        uint8_t y = baseline - 2; // wire runs through the middle of the text
        WireState state = wiring_state(&m->bus, i);
        uint8_t gap = m->gap[i];

        canvas_draw_str(canvas, 2, baseline, pins[i]);
        canvas_draw_str(canvas, 88, baseline, signals[i]);
        draw_connector(canvas, WIRE_X0 - 5, y);
        canvas_draw_box(canvas, WIRE_X1, y - 2, 3, 5);

        if(state == WireLive && gap == 0) {
            canvas_draw_line(canvas, WIRE_X0, y, WIRE_X1, y);
            // A pulse travelling Flipper -> sensor shows the link is live
            uint8_t span = WIRE_X1 - WIRE_X0;
            uint8_t px = WIRE_X0 + (uint8_t)((m->frame * 3 + i * 11) % span);
            canvas_draw_disc(canvas, px, y, 1);
        } else {
            // Open circuit: dashed stubs reaching towards each other. The gap
            // shrinks to nothing as the connection is made.
            for(uint8_t x = WIRE_X0; x <= mid - gap; x += 3) canvas_draw_dot(canvas, x, y);
            for(uint8_t x = WIRE_X1; x >= mid + gap; x -= 3) canvas_draw_dot(canvas, x, y);
            if(state == WireFault) {
                canvas_draw_line(canvas, mid - 3, y - 3, mid + 3, y + 3);
                canvas_draw_line(canvas, mid - 3, y + 3, mid + 3, y - 3);
            }
        }
    }

    if(m->sensor_seen) {
        canvas_draw_box(canvas, 0, 55, 128, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, 64, 62, AlignCenter, AlignBottom, "Sensor found! OK = scan");
        canvas_set_color(canvas, ColorBlack);
    } else if(m->bus.shorted) {
        canvas_draw_str_aligned(
            canvas, 64, 62, AlignCenter, AlignBottom, "SDA and SCL are shorted!");
    } else if(m->bus.health == I2CBusStuckLow) {
        canvas_draw_str_aligned(
            canvas, 64, 62, AlignCenter, AlignBottom, "Line stuck low - short?");
    } else if(m->bus.stray_pin) {
        // Only one line of room here, so alternate the quip and the fact.
        if((m->frame / 50) % 2) {
            canvas_draw_str_aligned(
                canvas, 64, 62, AlignCenter, AlignBottom, "Wrong hole - it happens.");
        } else {
            char buf[36];
            snprintf(buf, sizeof(buf), "Pull-ups are on pin %u", m->bus.stray_pin);
            canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, buf);
        }
    } else if((m->frame / 50) % 2) {
        // The rows above are already in the correct order to wire them up:
        // ground first to tie the references together, power next, signals
        // last into an already-powered chip. Say so while the user waits.
        canvas_draw_str_aligned(
            canvas, 64, 62, AlignCenter, AlignBottom, "Wire top-down: GND first");
    } else {
        // Fixed-width dots so the centred text does not jitter
        char buf[32];
        const char* dots = "   ";
        switch((m->frame / 8) % 4) {
        case 1:
            dots = ".  ";
            break;
        case 2:
            dots = ".. ";
            break;
        case 3:
            dots = "...";
            break;
        default:
            break;
        }
        snprintf(buf, sizeof(buf), "Waiting for sensor%s", dots);
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, buf);
    }
}

/* ---------------- Scan screen ---------------- */

// Four rows plus a hint bar: without the bar nothing tells the user that OK
// opens the detail screen, and a verdict with no explanation is just a word.
#define SCAN_LIST_ROWS 4

static void app_start_scan(FakeChipApp* app) {
    with_view_model(
        app->scan_view,
        ScanViewModel * m,
        {
            m->scanning = true;
            m->frame = 0;
            m->progress_addr = I2C_SCAN_ADDR_FIRST;
            m->found_count = 0;
            m->selected = 0;
            m->scroll = 0;
            m->bus = (I2CBusCheck){0};
            m->status_msg[0] = '\0';
        },
        true);
    app_switch_view(app, FakeChipViewScan);
    i2c_worker_start_scan(app->worker, i2c_settings_probe_timeout(&app->settings));
}

// The Right button, drawn as the play-style triangle users already associate
// with it. A bare letter "R" reads as part of the sentence, not as a key.
static void draw_right_key(Canvas* canvas, uint8_t x, uint8_t y) {
    // canvas_draw_triangle outlines; a play glyph has to be solid, so fill it
    // as vertical spans tapering to the point.
    for(uint8_t i = 0; i < 4; i++) {
        canvas_draw_line(canvas, x + i, y - 3 + i, x + i, y + 3 - i);
    }
}

// Bottom bar naming the two things the user can do here. Every screen that
// accepts input says so; the save state takes the bar over when it changes.
static void draw_action_bar(Canvas* canvas, const char* ok_action) {
    canvas_draw_box(canvas, 0, 55, 128, 9);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 62, ok_action);
    canvas_draw_str_aligned(canvas, 125, 62, AlignRight, AlignBottom, "save log");
    draw_right_key(canvas, 125 - canvas_string_width(canvas, "save log") - 9, 61);
    canvas_set_color(canvas, ColorBlack);
}

// 24x24 thumbs-up, XBM (least significant bit leftmost). Hand-drawn: a
// verdict the user is happy about deserves more than a word.
#define THUMB_W 24
#define THUMB_H 24
static const uint8_t thumbs_up_bits[] = {
    0x00, 0x0F, 0x00, 0x80, 0x10, 0x00, 0x80, 0x10, 0x00, 0x80, 0x10, 0x00,
    0x80, 0x10, 0x00, 0x80, 0x10, 0x00, 0x80, 0x10, 0x00, 0xC0, 0xE0, 0xFF,
    0x70, 0x00, 0x80, 0x18, 0x00, 0x80, 0x0C, 0x00, 0x80, 0x04, 0xC0, 0xFF,
    0x04, 0x00, 0x80, 0x04, 0x00, 0x80, 0x04, 0x00, 0x80, 0x04, 0xC0, 0xFF,
    0x04, 0x00, 0x80, 0x04, 0x00, 0x80, 0x04, 0x00, 0x80, 0x04, 0xC0, 0xFF,
    0x04, 0x00, 0x80, 0x04, 0x00, 0x80, 0x1C, 0x00, 0x80, 0xF0, 0xFF, 0xFF,
};

// A pictogram per verdict, 15px tall, drawn with primitives. At 1bpp an icon
// carries the mood faster than any word: a sealed badge reads as "good", a
// warning triangle as "trouble", before the text is even parsed.
static void draw_verdict_icon(Canvas* canvas, uint8_t cx, uint8_t cy, ChipVerdict verdict) {
    switch(verdict) {
    case VerdictGenuine:
        // Filled seal with a punched-out check
        canvas_draw_disc(canvas, cx, cy, 7);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_line(canvas, cx - 3, cy, cx - 1, cy + 3);
        canvas_draw_line(canvas, cx - 2, cy, cx, cy + 3);
        canvas_draw_line(canvas, cx - 1, cy + 3, cx + 4, cy - 3);
        canvas_draw_line(canvas, cx, cy + 3, cx + 5, cy - 3);
        canvas_set_color(canvas, ColorBlack);
        break;
    case VerdictWrongChip:
        // Warning triangle with an exclamation mark
        canvas_draw_line(canvas, cx, cy - 7, cx - 7, cy + 6);
        canvas_draw_line(canvas, cx, cy - 7, cx + 7, cy + 6);
        canvas_draw_line(canvas, cx - 7, cy + 6, cx + 7, cy + 6);
        canvas_draw_line(canvas, cx, cy - 3, cx, cy + 2);
        canvas_draw_dot(canvas, cx, cy + 4);
        break;
    case VerdictNoMatch:
    case VerdictUnknown:
        // Question mark in a ring
        canvas_draw_circle(canvas, cx, cy, 7);
        canvas_draw_line(canvas, cx - 2, cy - 3, cx + 1, cy - 4);
        canvas_draw_line(canvas, cx + 1, cy - 4, cx + 2, cy - 1);
        canvas_draw_line(canvas, cx + 2, cy - 1, cx, cy + 1);
        canvas_draw_dot(canvas, cx, cy + 4);
        break;
    case VerdictDetectedNoId:
        // Plug pictogram: it is here, that is all we know
        canvas_draw_circle(canvas, cx, cy, 7);
        canvas_draw_box(canvas, cx - 3, cy - 2, 6, 5);
        canvas_draw_line(canvas, cx - 2, cy - 5, cx - 2, cy - 3);
        canvas_draw_line(canvas, cx + 1, cy - 5, cx + 1, cy - 3);
        break;
    default:
        // Silent: a struck-through ring
        canvas_draw_circle(canvas, cx, cy, 7);
        canvas_draw_line(canvas, cx - 5, cy + 5, cx + 5, cy - 5);
        break;
    }
}

static void draw_down_key(Canvas* canvas, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < 4; i++) {
        canvas_draw_line(canvas, x - 3 + i, y + i, x + 3 - i, y + i);
    }
}

// Outcome-screen bar. Details hide behind Right so the raw hex never greets
// anyone; saving only appears where a saved file is actually worth having.
static void draw_hint_bar(Canvas* canvas, const char* right_action, bool offer_save) {
    canvas_draw_box(canvas, 0, 55, 128, 9);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    draw_right_key(canvas, 5, 60);
    canvas_draw_str(canvas, 12, 62, right_action);
    if(offer_save) {
        canvas_draw_str_aligned(canvas, 124, 62, AlignRight, AlignBottom, "save proof");
        draw_down_key(canvas, 124 - canvas_string_width(canvas, "save proof") - 8, 57);
    }
    canvas_set_color(canvas, ColorBlack);
}

// Yes/no bar for the expectation question.
static void draw_choice_bar(Canvas* canvas) {
    canvas_draw_box(canvas, 0, 55, 128, 9);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 62, "OK");
    canvas_draw_str(canvas, 20, 62, "yes");
    canvas_draw_str_aligned(canvas, 124, 62, AlignRight, AlignBottom, "no");
    draw_down_key(canvas, 124 - canvas_string_width(canvas, "no") - 8, 57);
    canvas_set_color(canvas, ColorBlack);
}

// Rotating radar sweep: a scan that visibly moves reads as alive even when
// every address NACKs.
static void draw_scan_spinner(Canvas* canvas, uint8_t cx, uint8_t cy, uint32_t frame) {
    canvas_draw_circle(canvas, cx, cy, 9);
    float a = (float)(frame % 32) / 32.0f * 2.0f * (float)M_PI;
    canvas_draw_line(
        canvas, cx, cy, cx + (int8_t)(sinf(a) * 8.0f), cy - (int8_t)(cosf(a) * 8.0f));
    canvas_draw_disc(canvas, cx, cy, 1);
}

static void scan_draw_callback(Canvas* canvas, void* model) {
    ScanViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(m->scanning) {
        canvas_draw_str(canvas, 2, 12, "Scanning bus...");
        draw_scan_spinner(canvas, 112, 20, m->frame);

        uint8_t span = I2C_SCAN_ADDR_LAST - I2C_SCAN_ADDR_FIRST;
        uint8_t done = m->progress_addr - I2C_SCAN_ADDR_FIRST;
        canvas_draw_frame(canvas, 4, 32, 92, 10);
        canvas_draw_box(canvas, 4, 32, (uint16_t)done * 92 / span, 10);

        canvas_set_font(canvas, FontSecondary);
        char buf[28];
        snprintf(buf, sizeof(buf), "addr 0x%02X   found: %u", m->progress_addr, m->found_count);
        canvas_draw_str(canvas, 4, 52, buf);
        return;
    }

    if(m->found_count == 0) {
        canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, "No devices found");
        canvas_set_font(canvas, FontSecondary);

        // Hints keyed to what the electrical check actually saw
        const char* l1;
        const char* l2;
        const char* l3;
        switch(m->bus.health) {
        case I2CBusStuckLow:
            l1 = m->bus.scl_stuck ?
                     (m->bus.sda_stuck ? "Both lines held LOW" : "SCL (pin 16) held LOW") :
                     "SDA (pin 15) held LOW";
            l2 = "Shorted, or a hung chip.";
            l3 = "Unplug and re-seat it.";
            break;
        case I2CBusFloating:
            // FontSecondary is ~5px per character, so a line must stay under
            // ~25 characters or it is clipped at both edges.
            l1 = "No pull-ups on the bus.";
            l2 = "No power, or not wired.";
            l3 = "Check pins 8, 9, 15, 16.";
            break;
        default:
            l1 = "Bus is electrically OK.";
            l2 = "Wrong address, SDA/SCL";
            l3 = "swapped, or dead chip.";
            break;
        }
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignBottom, l1);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignBottom, l2);
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignBottom, l3);
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "OK = rescan");
        return;
    }

    char buf[40];

    // One device is the normal case: the user is checking a single sensor and
    // wants an answer, not a table. Give the whole screen to the verdict.
    if(m->found_count == 1) {
        const I2CFoundDevice* dev = &m->found[0];
        ChipVerdict v = dev->ident.verdict;
        const char* name = dev->ident.chip ? dev->ident.chip->name : "Unknown chip";
        const char* kind = dev->ident.chip ? dev->ident.chip->kind : NULL;

        if(m->answer == AnswerAsking) {
            // What it is, said in one breath: badge, part number and what the
            // part does — nobody should have to search for the number.
            draw_verdict_icon(canvas, 12, 17, v);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 26, 15, name);
            canvas_set_font(canvas, FontSecondary);
            if(kind) canvas_draw_str(canvas, 26, 25, kind);

            snprintf(buf, sizeof(buf), "%s at 0x%02X", chip_verdict_headline(v), dev->addr);
            canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignBottom, buf);
            canvas_draw_str_aligned(
                canvas, 64, 49, AlignCenter, AlignBottom, "Is this what you bought?");
            draw_choice_bar(canvas);
            return;
        }

        // Saving a file and not saying where it went is not saving it. Show
        // the name and the folder until the user presses Back.
        if(m->saved_name[0]) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignBottom, "REPORT SAVED");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignBottom, m->saved_name);
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignBottom, "On the SD card, in");
            canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignBottom, "apps_data/");
            canvas_draw_str_aligned(
                canvas, 64, 58, AlignCenter, AlignBottom, "fake_chip_detector");
            return;
        }

        bool good = (m->answer == AnswerExpected) && chip_verdict_is_good(v);

        if(good) {
            // The whole point of the app, and the moment to be generous about
            // it: a thumb, a headline, and no hex anywhere in sight.
            canvas_draw_xbm(canvas, 4, 7, THUMB_W, THUMB_H, thumbs_up_bits);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 36, 20, "ALL GOOD");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 36, 30, "Real deal.");
            canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignBottom, name);
            if(kind) canvas_draw_str_aligned(canvas, 64, 51, AlignCenter, AlignBottom, kind);
            draw_hint_bar(canvas, "details", false);
        } else {
            draw_verdict_icon(canvas, 12, 20, VerdictWrongChip);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 28, 16, "NOT YOURS");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 28, 26, "You were sold");
            snprintf(buf, sizeof(buf), "a %s", kind ? kind : name);
            canvas_draw_str(canvas, 28, 35, buf);
            canvas_draw_str_aligned(
                canvas, 64, 48, AlignCenter, AlignBottom, "Save proof for the seller.");
            draw_hint_bar(canvas, "details", true);
        }
        return;
    }

    snprintf(buf, sizeof(buf), "Found %u devices", m->found_count);
    canvas_draw_str(canvas, 2, 10, buf);
    canvas_set_font(canvas, FontSecondary);

    for(uint8_t row = 0; row < SCAN_LIST_ROWS; row++) {
        uint8_t idx = m->scroll + row;
        if(idx >= m->found_count) break;
        uint8_t y = 22 + row * 10;
        const I2CFoundDevice* dev = &m->found[idx];
        const char* name = dev->ident.chip ? dev->ident.chip->name : "UNKNOWN";
        snprintf(
            buf,
            sizeof(buf),
            "0x%02X %s %s",
            dev->addr,
            name,
            chip_verdict_short_str(dev->ident.verdict));
        if(idx == m->selected) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 4, y, buf);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 4, y, buf);
        }
    }

    // More results than fit: say so rather than silently hiding them
    if(m->found_count > SCAN_LIST_ROWS) {
        snprintf(buf, sizeof(buf), "%u/%u", m->selected + 1, m->found_count);
        canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, buf);
    }

    draw_action_bar(canvas, "OK: details");
}

/* Writes a snapshot of the scan results to /ext/apps_data/fake_chip_detector/.
 * Takes a copy rather than the live model: SD writes can stall for seconds
 * and must never run while the view-model mutex is held. */
static bool scan_save_log(
    const I2CFoundDevice* found,
    uint8_t count,
    bool disputed,
    char* out_name,
    size_t out_name_size) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = false;

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char name[32];
    snprintf(
        name,
        sizeof(name),
        "scan_%04u%02u%02u_%02u%02u%02u.txt",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);
    if(out_name) snprintf(out_name, out_name_size, "%s", name);

    FuriString* path = furi_string_alloc_printf(APP_DATA_PATH("%s"), name);
    storage_common_resolve_path_and_ensure_app_directory(storage, path);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* text = furi_string_alloc();
        report_build(text, found, count, disputed, &dt);
        size_t len = furi_string_size(text);
        ok = storage_file_write(file, furi_string_get_cstr(text), len) == len;
        furi_string_free(text);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(path);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static void app_show_report(FakeChipApp* app, bool disputed);

// Snapshots the results and writes the report outside the model lock: SD
// writes can stall for seconds and must never hold up the GUI.
static void app_save_log(FakeChipApp* app, bool disputed) {
    I2CFoundDevice* snapshot = malloc(sizeof(I2CFoundDevice) * I2C_SCAN_MAX_FOUND);
    uint8_t count = 0;
    with_view_model(
        app->scan_view,
        ScanViewModel * m,
        {
            count = m->found_count;
            if(count > I2C_SCAN_MAX_FOUND) count = I2C_SCAN_MAX_FOUND;
            memcpy(snapshot, m->found, count * sizeof(I2CFoundDevice));
        },
        false);

    char name[32] = {0};
    bool saved = scan_save_log(snapshot, count, disputed, name, sizeof(name));
    free(snapshot);

    with_view_model(
        app->scan_view,
        ScanViewModel * m,
        {
            snprintf(
                m->status_msg,
                sizeof(m->status_msg),
                "%s",
                saved ? "Log saved to SD" : "SD write failed!");
            // Drives the confirmation screen: a file the user cannot find is
            // the same as no file at all.
            snprintf(m->saved_name, sizeof(m->saved_name), "%s", saved ? name : "");
        },
        true);
}

static I2CNotifyKind verdict_notify_kind(ChipVerdict verdict) {
    switch(verdict) {
    case VerdictGenuine:
        return I2CNotifyGenuine;
    case VerdictWrongChip:
    case VerdictNoAnswer:
        return I2CNotifyBad;
    case VerdictUnknown:
        return I2CNotifyAttention;
    default:
        return I2CNotifyNeutral;
    }
}

static bool scan_input_callback(InputEvent* event, void* context) {
    FakeChipApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = false;
    bool rescan = false;
    bool open_detail = false;
    bool do_save = false;
    bool answered_wrong = false;
    bool disputed = false;
    bool show_report = false;

    with_view_model(
        app->scan_view,
        ScanViewModel * m,
        {
            if(!m->scanning) { // no navigation while scanning
                if(m->saved_name[0]) {
                    // any key dismisses the save confirmation
                    m->saved_name[0] = '\0';
                    consumed = true;
                } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
                    if(m->found_count == 0) {
                        rescan = true;
                    } else if(m->found_count == 1 && m->answer == AnswerAsking) {
                        m->answer = AnswerExpected; // "yes, that is what I bought"
                    } else if(m->found_count == 1) {
                        show_report = true;
                        disputed = (m->answer == AnswerNotWhatIOrdered);
                    } else {
                        open_detail = true;
                    }
                    consumed = true;
                } else if(
                    event->key == InputKeyDown && m->found_count == 1 &&
                    m->answer == AnswerAsking && event->type == InputTypeShort) {
                    m->answer = AnswerNotWhatIOrdered;
                    answered_wrong = true;
                    consumed = true;
                } else if(
                    event->key == InputKeyDown && m->found_count == 1 &&
                    m->answer != AnswerAsking && event->type == InputTypeShort) {
                    do_save = true;
                    disputed = (m->answer == AnswerNotWhatIOrdered);
                    consumed = true;
                } else if(event->key == InputKeyUp && m->found_count > 0) {
                    if(m->selected > 0) m->selected--;
                    if(m->selected < m->scroll) m->scroll = m->selected;
                    consumed = true;
                } else if(event->key == InputKeyDown && m->found_count > 0) {
                    if(m->selected + 1 < m->found_count) m->selected++;
                    if(m->selected >= m->scroll + SCAN_LIST_ROWS)
                        m->scroll = m->selected - SCAN_LIST_ROWS + 1;
                    consumed = true;
                } else if(
                    event->key == InputKeyRight && m->found_count > 0 &&
                    event->type == InputTypeShort) {
                    if(m->found_count == 1 && m->answer != AnswerAsking) {
                        open_detail = true; // hex lives behind Right, never up front
                    } else {
                        do_save = true;
                    }
                    disputed = (m->answer == AnswerNotWhatIOrdered);
                    consumed = true;
                }
            }
        },
        consumed);

    if(answered_wrong) i2c_notify_play(app->notifications, I2CNotifyBad);

    if(show_report) app_show_report(app, disputed);

    if(do_save) {
        app_save_log(app, disputed);
        i2c_notify_play(app->notifications, I2CNotifyNeutral);
    }

    if(rescan) app_start_scan(app);

    if(open_detail) {
        I2CFoundDevice selected_dev;
        bool have = false;
        with_view_model(
            app->scan_view,
            ScanViewModel * m,
            {
                if(m->selected < m->found_count) {
                    selected_dev = m->found[m->selected];
                    have = true;
                }
            },
            false);
        if(have) {
            with_view_model(
                app->detail_view, DetailViewModel * dm, { dm->device = selected_dev; }, true);
            app_switch_view(app, FakeChipViewDetail);
            i2c_notify_play(app->notifications, verdict_notify_kind(selected_dev.ident.verdict));
        }
    }
    return consumed;
}

static void worker_event_callback(I2CWorkerEvent event, void* context) {
    FakeChipApp* app = context;

    if(event == I2CWorkerEventScanProgress || event == I2CWorkerEventScanDone) {
        bool done = (event == I2CWorkerEventScanDone);
        uint8_t count = 0;
        bool any_genuine = false, any_bad = false;
        I2CBusCheck bus;
        i2c_worker_get_bus(app->worker, &bus);

        with_view_model(
            app->scan_view,
            ScanViewModel * m,
            {
                m->progress_addr = i2c_worker_get_progress(app->worker);
                m->found_count = i2c_worker_get_found(app->worker, m->found, I2C_SCAN_MAX_FOUND);
                m->bus = bus;
                if(done) {
                    m->scanning = false;
                    m->selected = 0;
                    m->scroll = 0;
                    count = m->found_count;
                    for(uint8_t i = 0; i < count; i++) {
                        ChipVerdict v = m->found[i].ident.verdict;
                        if(v == VerdictGenuine) any_genuine = true;
                        if(v == VerdictWrongChip || v == VerdictNoAnswer) any_bad = true;
                    }
                }
            },
            true);

        if(done) {
            I2CNotifyKind kind;
            if(count == 0) {
                kind = I2CNotifyAttention; // nothing to check — the user must act
            } else if(any_bad) {
                kind = I2CNotifyBad;
            } else if(any_genuine) {
                kind = I2CNotifyGenuine;
            } else {
                kind = I2CNotifyNeutral;
            }
            i2c_notify_play(app->notifications, kind);

            if(app->settings.autosave && count > 0) app_save_log(app, false);
        }
    } else if(event == I2CWorkerEventLiveUpdate) {
        uint8_t cal = 0;
        bool running = false;
        with_view_model(
            app->live_view,
            LiveViewModel * m,
            {
                i2c_worker_get_live(app->worker, &m->data);
                cal = m->data.mag_cal;
                running = (m->data.status == I2CLiveStatusRunning);
            },
            true);
        // Chime once, on the transition to fully calibrated
        if(running && cal == 3 && app->last_mag_cal != 3) {
            i2c_notify_play(app->notifications, I2CNotifyCalibrated);
        }
        if(running) app->last_mag_cal = cal;
    } else if(event == I2CWorkerEventBusUpdate) {
        I2CBusCheck bus;
        i2c_worker_get_bus(app->worker, &bus);
        bool became_connected = false;
        bool became_wrong = false;
        with_view_model(
            app->wiring_view,
            WiringViewModel * m,
            {
                bool seen = (bus.health == I2CBusOk) && !bus.shorted;
                bool wrong = bus.shorted || bus.stray_pin ||
                             bus.health == I2CBusStuckLow;
                bool was_wrong = m->bus.shorted || m->bus.stray_pin ||
                                 m->bus.health == I2CBusStuckLow;
                if(seen && !m->sensor_seen) became_connected = true;
                if(wrong && !was_wrong) became_wrong = true;
                m->bus = bus;
                m->sensor_seen = seen;
            },
            true);
        // Chirp once per transition, never on every poll
        if(became_connected) i2c_notify_play(app->notifications, I2CNotifyGenuine);
        if(became_wrong) i2c_notify_play(app->notifications, I2CNotifyAttention);
    }
}

/* ---------------- Wiring enter/exit ---------------- */

static void wiring_enter_callback(void* context) {
    FakeChipApp* app = context;
    with_view_model(
        app->wiring_view,
        WiringViewModel * m,
        {
            m->frame = 0;
            m->sensor_seen = false;
            m->bus = (I2CBusCheck){0};
            for(uint8_t i = 0; i < 4; i++) m->gap[i] = WIRE_GAP_MAX;
        },
        true);
    i2c_worker_watch_start(app->worker);
}

static void wiring_exit_callback(void* context) {
    FakeChipApp* app = context;
    i2c_worker_watch_stop(app->worker);
}

/* ---------------- Detail screen ---------------- */

static void detail_draw_callback(Canvas* canvas, void* model) {
    DetailViewModel* m = model;
    const I2CFoundDevice* dev = &m->device;
    canvas_clear(canvas);

    char buf[48];
    canvas_set_font(canvas, FontPrimary);
    snprintf(
        buf,
        sizeof(buf),
        "0x%02X  %s",
        dev->addr,
        dev->ident.chip ? dev->ident.chip->name : "UNKNOWN");
    canvas_draw_str(canvas, 2, 10, buf);

    canvas_set_font(canvas, FontSecondary);
    bool any_read_failed = false;
    uint8_t y = 20;
    for(uint8_t r = 0; r < dev->ident.read_count && r < CHIP_MAX_CHECKS; r++) {
        const IdReadResult* rr = &dev->ident.reads[r];
        uint8_t digits = rr->wide ? 4 : 2;
        uint8_t rdigits = rr->reg16 ? 4 : 2;
        if(!rr->read_ok) {
            any_read_failed = true;
            snprintf(buf, sizeof(buf), "0x%0*X: read FAILED", rdigits, rr->reg);
        } else if(rr->has_expected) {
            snprintf(
                buf,
                sizeof(buf),
                "0x%0*X: %0*X exp %0*X %s",
                rdigits,
                rr->reg,
                digits,
                rr->actual,
                digits,
                rr->expected,
                rr->match ? "OK" : "BAD");
        } else {
            snprintf(buf, sizeof(buf), "0x%0*X: %02X", rdigits, rr->reg, rr->actual);
        }
        canvas_draw_str(canvas, 2, y, buf);
        y += 9;
    }

    if(dev->ident.read_count == 0) {
        canvas_draw_str(canvas, 2, 20, "This chip has no ID reg:");
        canvas_draw_str(canvas, 2, 29, "only presence is proven.");
        y = 38;
    }
    if(any_read_failed && y <= 45) {
        canvas_draw_str(canvas, 2, y, "Answers, but reads fail.");
        y += 9;
    }
    if(dev->ident.chip && dev->ident.chip->note && y <= 51) {
        snprintf(buf, sizeof(buf), "! %s", dev->ident.chip->note);
        canvas_draw_str(canvas, 2, y, buf);
    }

    canvas_draw_box(canvas, 0, 54, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(
        canvas, 64, 62, AlignCenter, AlignBottom, chip_verdict_str(dev->ident.verdict));
    canvas_set_color(canvas, ColorBlack);
}

// Navigation callbacks receive the *view's* context. For Widget and
// VariableItemList that is the module instance, not the app, so these must
// never dereference it — state changes belong in the exit callbacks of the
// views we own.
static uint32_t nav_to_scan(void* context) {
    UNUSED(context);
    return FakeChipViewScan;
}

/* ---------------- BNO055 live test ---------------- */

// Lemniscate traced by a moving dot: the figure-8 motion the magnetometer
// needs for calibration, shown instead of described.
static void draw_figure8(Canvas* canvas, uint8_t cx, uint8_t cy, uint32_t frame) {
    const float rx = 20.0f, ry = 8.0f;
    for(uint8_t i = 0; i < 32; i++) {
        float t = (float)i / 32.0f * 2.0f * (float)M_PI;
        canvas_draw_dot(canvas, cx + (int8_t)(rx * sinf(t)), cy + (int8_t)(ry * sinf(2 * t)));
    }
    float t = (float)(frame % 48) / 48.0f * 2.0f * (float)M_PI;
    canvas_draw_disc(
        canvas, cx + (int8_t)(rx * sinf(t)), cy + (int8_t)(ry * sinf(2 * t)), 2);
}

static void live_draw_callback(Canvas* canvas, void* model) {
    LiveViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    switch(m->data.status) {
    case I2CLiveStatusSearching:
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignBottom, "Looking for BNO055");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, "Probing 0x28 and 0x29");
        draw_scan_spinner(canvas, 64, 40, m->frame);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Plug it in - I'll wait");
        return;
    case I2CLiveStatusInit: {
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignBottom, "Starting NDOF mode");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignBottom, "Sensor fusion warm-up");
        uint8_t w = (uint8_t)((m->frame % 20) * 100 / 20);
        canvas_draw_frame(canvas, 14, 34, 100, 8);
        canvas_draw_box(canvas, 14, 34, w, 8);
        return;
    }
    case I2CLiveStatusLost:
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignBottom, "Sensor dropped off!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 28, AlignCenter, AlignBottom, "It replied, then stopped.");
        canvas_draw_str_aligned(
            canvas, 64, 38, AlignCenter, AlignBottom, "Check 3V3 and wires.");
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignBottom, "Retrying...");
        return;
    case I2CLiveStatusRunning:
        break;
    }

    // Heading arrives in 1/16 degree steps; integer math keeps printf light.
    int32_t raw = m->data.heading_raw;
    if(raw < 0) raw += 360 * 16;
    int32_t deg = raw / 16;
    int32_t frac = (raw % 16) * 10 / 16;

    char buf[24];
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%ld.%ld", (long)deg, (long)frac);
    canvas_draw_str(canvas, 2, 26, buf);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 36, "deg");

    // Compass: 0 deg = north = up, needle follows the sensor
    const uint8_t cx = 100, cy = 24, r = 18;
    canvas_draw_circle(canvas, cx, cy, r);
    canvas_draw_str_aligned(canvas, cx, cy - r + 6, AlignCenter, AlignBottom, "N");
    float a = (float)raw / 16.0f * ((float)M_PI / 180.0f);
    int8_t dx = (int8_t)(sinf(a) * (r - 4));
    int8_t dy = (int8_t)(-cosf(a) * (r - 4));
    canvas_draw_line(canvas, cx, cy, cx + dx, cy + dy);
    canvas_draw_disc(canvas, cx + dx, cy + dy, 2);

    snprintf(buf, sizeof(buf), "MAG CAL %u/3", m->data.mag_cal);
    canvas_draw_str(canvas, 2, 47, buf);
    for(uint8_t i = 0; i < 3; i++) {
        uint8_t bx = 58 + i * 9;
        if(i < m->data.mag_cal) {
            canvas_draw_box(canvas, bx, 40, 7, 7);
        } else {
            canvas_draw_frame(canvas, bx, 40, 7, 7);
        }
    }

    if(m->data.mag_cal < 3) {
        draw_figure8(canvas, 24, 57, m->frame);
        canvas_draw_str(canvas, 50, 60, "Rotate in figure-8");
    } else {
        canvas_draw_box(canvas, 0, 51, 128, 13);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, 64, 61, AlignCenter, AlignBottom, "CALIBRATED - now spin it");
        canvas_set_color(canvas, ColorBlack);
    }
}

static void live_enter_callback(void* context) {
    FakeChipApp* app = context;
    app->last_mag_cal = 0;
    with_view_model(
        app->live_view,
        LiveViewModel * m,
        {
            m->data = (I2CLiveData){.status = I2CLiveStatusSearching};
            m->frame = 0;
        },
        true);
    i2c_worker_live_start(app->worker);
}

static void live_exit_callback(void* context) {
    FakeChipApp* app = context;
    i2c_worker_live_stop(app->worker);
}

/* ---------------- Settings ---------------- */

static const char* const on_off_names[] = {"OFF", "ON"};

static void settings_apply(FakeChipApp* app) {
    i2c_notify_apply_settings(&app->settings);
    i2c_settings_save(&app->settings);
}

static void settings_sound_changed(VariableItem* item) {
    FakeChipApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off_names[idx]);
    app->settings.sound = idx;
    settings_apply(app);
}

static void settings_vibro_changed(VariableItem* item) {
    FakeChipApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off_names[idx]);
    app->settings.vibro = idx;
    settings_apply(app);
    // Buzz on enable so the setting demonstrates itself
    if(idx) i2c_notify_play(app->notifications, I2CNotifyNeutral);
}

static void settings_led_changed(VariableItem* item) {
    FakeChipApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off_names[idx]);
    app->settings.led = idx;
    settings_apply(app);
    if(idx) i2c_notify_play(app->notifications, I2CNotifyNeutral);
}

static void settings_backlight_changed(VariableItem* item) {
    FakeChipApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off_names[idx]);
    app->settings.backlight = idx;
    notification_message(
        app->notifications,
        idx ? &sequence_display_backlight_enforce_on :
              &sequence_display_backlight_enforce_auto);
    settings_apply(app);
}

static void settings_timeout_changed(VariableItem* item) {
    FakeChipApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, i2c_settings_timeout_names[idx]);
    app->settings.probe_timeout_idx = idx;
    settings_apply(app);
}

static void settings_autosave_changed(VariableItem* item) {
    FakeChipApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off_names[idx]);
    app->settings.autosave = idx;
    settings_apply(app);
}

static void settings_build(FakeChipApp* app) {
    VariableItemList* list = app->settings_list;
    VariableItem* item;

    item = variable_item_list_add(list, "Sound", 2, settings_sound_changed, app);
    variable_item_set_current_value_index(item, app->settings.sound);
    variable_item_set_current_value_text(item, on_off_names[app->settings.sound]);

    item = variable_item_list_add(list, "Vibration", 2, settings_vibro_changed, app);
    variable_item_set_current_value_index(item, app->settings.vibro);
    variable_item_set_current_value_text(item, on_off_names[app->settings.vibro]);

    item = variable_item_list_add(list, "LED", 2, settings_led_changed, app);
    variable_item_set_current_value_index(item, app->settings.led);
    variable_item_set_current_value_text(item, on_off_names[app->settings.led]);

    item = variable_item_list_add(list, "Keep backlight", 2, settings_backlight_changed, app);
    variable_item_set_current_value_index(item, app->settings.backlight);
    variable_item_set_current_value_text(item, on_off_names[app->settings.backlight]);

    item = variable_item_list_add(
        list, "Probe speed", I2C_SETTINGS_TIMEOUT_COUNT, settings_timeout_changed, app);
    variable_item_set_current_value_index(item, app->settings.probe_timeout_idx);
    variable_item_set_current_value_text(
        item, i2c_settings_timeout_names[app->settings.probe_timeout_idx]);

    item = variable_item_list_add(list, "Auto-save log", 2, settings_autosave_changed, app);
    variable_item_set_current_value_index(item, app->settings.autosave);
    variable_item_set_current_value_text(item, on_off_names[app->settings.autosave]);
}


/* ---------------- Supported chips ---------------- */

#define CHIPS_LIST_ROWS 4

// Answers "what does this thing actually know?", and doubles as the place
// where every name and description is shown at full width — if one of them
// were too long for the screen, it would be obvious here.
static void chips_draw_callback(Canvas* canvas, void* model) {
    ChipsViewModel* m = model;
    size_t total = chip_db_count();
    canvas_clear(canvas);

    char buf[24];
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Known chips");
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%u/%u", (unsigned)m->selected + 1, (unsigned)total);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, buf);

    uint16_t first = 0;
    if(m->selected >= CHIPS_LIST_ROWS) first = m->selected - CHIPS_LIST_ROWS + 1;

    for(uint8_t row = 0; row < CHIPS_LIST_ROWS; row++) {
        size_t idx = first + row;
        if(idx >= total) break;
        const ChipEntry* chip = chip_db_get(idx);
        uint8_t y = 22 + row * 10;
        bool sel = (idx == m->selected);
        if(sel) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 4, y, chip->name);
        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    // The description gets a line of its own. Packing it beside the name made
    // the two collide as soon as either was long.
    const ChipEntry* current = chip_db_get(m->selected);
    if(current) {
        canvas_draw_box(canvas, 0, 55, 128, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, current->kind);
        canvas_set_color(canvas, ColorBlack);
    }
}

static bool chips_input_callback(InputEvent* event, void* context) {
    FakeChipApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    bool consumed = false;
    with_view_model(
        app->chips_view,
        ChipsViewModel * m,
        {
            size_t total = chip_db_count();
            if(event->key == InputKeyUp && m->selected > 0) {
                m->selected--;
                consumed = true;
            } else if(event->key == InputKeyDown && (size_t)(m->selected + 1) < total) {
                m->selected++;
                consumed = true;
            }
        },
        consumed);
    return consumed;
}


/* ---------------- Report viewer ---------------- */

// The file on the SD card is for later. What matters at the front door is a
// screen you can hand to the courier, so the same text goes on the display.
static void app_show_report(FakeChipApp* app, bool disputed) {
    I2CFoundDevice* snapshot = malloc(sizeof(I2CFoundDevice) * I2C_SCAN_MAX_FOUND);
    uint8_t count = 0;
    with_view_model(
        app->scan_view,
        ScanViewModel * m,
        {
            count = m->found_count;
            if(count > I2C_SCAN_MAX_FOUND) count = I2C_SCAN_MAX_FOUND;
            memcpy(snapshot, m->found, count * sizeof(I2CFoundDevice));
        },
        false);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    report_build(app->report_text, snapshot, count, disputed, &dt);
    free(snapshot);

    text_box_reset(app->report_box);
    text_box_set_font(app->report_box, TextBoxFontText);
    text_box_set_text(app->report_box, furi_string_get_cstr(app->report_text));
    app_switch_view(app, FakeChipViewReport);
}

/* ---------------- Animation tick ---------------- */

// Animation runs on its own thread rather than a FuriTimer: a timer callback
// executes on the shared FreeRTOS timer daemon, and taking the view-model
// mutex plus queueing a GUI redraw from there can stall every timer in the
// firmware — including the ones the USB and storage services depend on.
static int32_t anim_thread_worker(void* context) {
    FakeChipApp* app = context;

    while(!app->anim_stop) {
        furi_delay_ms(ANIM_PERIOD_MS);
        if(app->anim_stop) break;

        switch(app->current_view) {
        case FakeChipViewWiring:
            with_view_model(
                app->wiring_view,
                WiringViewModel * m,
                {
                    m->frame++;
                    // Ease each wire's break shut as its line comes alive,
                    // and back open if it is unplugged again.
                    for(uint8_t i = 0; i < 4; i++) {
                        bool live = wiring_state(&m->bus, i) == WireLive;
                        if(live && m->gap[i] > 0) {
                            m->gap[i]--;
                        } else if(!live && m->gap[i] < WIRE_GAP_MAX) {
                            m->gap[i]++;
                        }
                    }
                },
                true);
            break;
        case FakeChipViewScan:
            with_view_model(app->scan_view, ScanViewModel * m, { m->frame++; }, true);
            break;
        case FakeChipViewLive:
            with_view_model(app->live_view, LiveViewModel * m, { m->frame++; }, true);
            break;
        default:
            break; // menus and static screens need no ticks
        }
    }
    return 0;
}

/* ---------------- Menu ---------------- */

static void menu_callback(void* context, uint32_t index) {
    FakeChipApp* app = context;
    switch(index) {
    case MenuIndexWiring:
        app_switch_view(app, FakeChipViewWiring);
        break;
    case MenuIndexScan:
        app_start_scan(app);
        break;
    case MenuIndexLiveTest:
        app_switch_view(app, FakeChipViewLive);
        break;
    case MenuIndexSettings:
        app_switch_view(app, FakeChipViewSettings);
        break;
    case MenuIndexChips:
        app_switch_view(app, FakeChipViewChips);
        break;
    case MenuIndexAbout:
        app_switch_view(app, FakeChipViewAbout);
        break;
    }
}

static bool wiring_input_callback(InputEvent* event, void* context) {
    FakeChipApp* app = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        app_start_scan(app);
        return true;
    }
    return false;
}

static uint32_t nav_to_menu(void* context) {
    UNUSED(context);
    return FakeChipViewMenu;
}

static void scan_exit_callback(void* context) {
    FakeChipApp* app = context;
    app->current_view = FakeChipViewMenu;
    i2c_worker_abort_scan(app->worker); // never leave a sweep running behind us
}

static void detail_enter_callback(void* context) {
    FakeChipApp* app = context;
    app->current_view = FakeChipViewDetail;
}

static uint32_t nav_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

/* ---------------- App lifecycle ---------------- */

static FakeChipApp* fake_chip_app_alloc(void) {
    FakeChipApp* app = malloc(sizeof(FakeChipApp));
    memset(app, 0, sizeof(FakeChipApp));

    i2c_settings_load(&app->settings);
    i2c_notify_apply_settings(&app->settings);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    app->current_view = FakeChipViewMenu;

    if(app->settings.backlight) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }

    app->worker = i2c_worker_alloc();
    i2c_worker_set_callback(app->worker, worker_event_callback, app);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Fake Chip Detector");
    submenu_add_item(app->submenu, "How to wire", MenuIndexWiring, menu_callback, app);
    submenu_add_item(app->submenu, "Scan bus", MenuIndexScan, menu_callback, app);
    submenu_add_item(app->submenu, "BNO055 live test", MenuIndexLiveTest, menu_callback, app);
    submenu_add_item(app->submenu, "Settings", MenuIndexSettings, menu_callback, app);
    submenu_add_item(app->submenu, "Known chips", MenuIndexChips, menu_callback, app);
    submenu_add_item(app->submenu, "About", MenuIndexAbout, menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), nav_exit);
    view_dispatcher_add_view(
        app->view_dispatcher, FakeChipViewMenu, submenu_get_view(app->submenu));

    app->wiring_view = view_alloc();
    view_set_context(app->wiring_view, app);
    view_allocate_model(app->wiring_view, ViewModelTypeLocking, sizeof(WiringViewModel));
    view_set_draw_callback(app->wiring_view, wiring_draw_callback);
    view_set_input_callback(app->wiring_view, wiring_input_callback);
    view_set_enter_callback(app->wiring_view, wiring_enter_callback);
    view_set_exit_callback(app->wiring_view, wiring_exit_callback);
    view_set_previous_callback(app->wiring_view, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, FakeChipViewWiring, app->wiring_view);

    app->scan_view = view_alloc();
    view_set_context(app->scan_view, app);
    view_allocate_model(app->scan_view, ViewModelTypeLocking, sizeof(ScanViewModel));
    view_set_draw_callback(app->scan_view, scan_draw_callback);
    view_set_input_callback(app->scan_view, scan_input_callback);
    view_set_exit_callback(app->scan_view, scan_exit_callback);
    view_set_previous_callback(app->scan_view, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, FakeChipViewScan, app->scan_view);

    app->detail_view = view_alloc();
    view_set_context(app->detail_view, app);
    view_allocate_model(app->detail_view, ViewModelTypeLocking, sizeof(DetailViewModel));
    view_set_draw_callback(app->detail_view, detail_draw_callback);
    view_set_enter_callback(app->detail_view, detail_enter_callback);
    view_set_previous_callback(app->detail_view, nav_to_scan);
    view_dispatcher_add_view(app->view_dispatcher, FakeChipViewDetail, app->detail_view);

    app->live_view = view_alloc();
    view_set_context(app->live_view, app);
    view_allocate_model(app->live_view, ViewModelTypeLocking, sizeof(LiveViewModel));
    view_set_draw_callback(app->live_view, live_draw_callback);
    view_set_enter_callback(app->live_view, live_enter_callback);
    view_set_exit_callback(app->live_view, live_exit_callback);
    view_set_previous_callback(app->live_view, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, FakeChipViewLive, app->live_view);

    app->settings_list = variable_item_list_alloc();
    settings_build(app);
    view_set_previous_callback(variable_item_list_get_view(app->settings_list), nav_to_menu);
    view_dispatcher_add_view(
        app->view_dispatcher,
        FakeChipViewSettings,
        variable_item_list_get_view(app->settings_list));

    app->chips_view = view_alloc();
    view_set_context(app->chips_view, app);
    view_allocate_model(app->chips_view, ViewModelTypeLocking, sizeof(ChipsViewModel));
    view_set_draw_callback(app->chips_view, chips_draw_callback);
    view_set_input_callback(app->chips_view, chips_input_callback);
    view_set_previous_callback(app->chips_view, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, FakeChipViewChips, app->chips_view);

    app->report_text = furi_string_alloc();
    app->report_box = text_box_alloc();
    view_set_previous_callback(text_box_get_view(app->report_box), nav_to_scan);
    view_dispatcher_add_view(
        app->view_dispatcher, FakeChipViewReport, text_box_get_view(app->report_box));

    app->about_widget = widget_alloc();
    widget_add_string_element(
        app->about_widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "Fake Chip Detector");
    widget_add_string_element(
        app->about_widget, 64, 20, AlignCenter, AlignTop, FontSecondary, "Spot fake I2C sensors");
    widget_add_string_element(
        app->about_widget, 64, 30, AlignCenter, AlignTop, FontSecondary, "by their ID registers.");
    {
        static char db_line[32];
        snprintf(db_line, sizeof(db_line), "%u chips known", (unsigned)chip_db_count());
        widget_add_string_element(
            app->about_widget, 64, 42, AlignCenter, AlignTop, FontSecondary, db_line);
    }
    widget_add_string_element(
        app->about_widget,
        64,
        52,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "pin16 SCL/15 SDA - MIT");
    view_set_previous_callback(widget_get_view(app->about_widget), nav_to_menu);
    view_dispatcher_add_view(
        app->view_dispatcher, FakeChipViewAbout, widget_get_view(app->about_widget));

    app->anim_stop = false;
    app->anim_thread = furi_thread_alloc_ex("FakeChipAnim", 1024, anim_thread_worker, app);
    furi_thread_start(app->anim_thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, FakeChipViewMenu);
    return app;
}

static void fake_chip_app_free(FakeChipApp* app) {
    // Animation thread first: it touches the view models
    app->anim_stop = true;
    furi_thread_join(app->anim_thread);
    furi_thread_free(app->anim_thread);
    // Then the worker: joining it guarantees no more callbacks into the views
    i2c_worker_free(app->worker);

    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);

    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewWiring);
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewScan);
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewDetail);
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewLive);
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewChips);
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewReport);
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewAbout);
    submenu_free(app->submenu);
    view_free(app->wiring_view);
    view_free(app->scan_view);
    view_free(app->detail_view);
    view_free(app->live_view);
    view_free(app->chips_view);
    text_box_free(app->report_box);
    furi_string_free(app->report_text);
    variable_item_list_free(app->settings_list);
    widget_free(app->about_widget);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t fake_chip_detector_app(void* p) {
    UNUSED(p);
    // The sensor is powered from pin 9: make sure the external 3.3V rail is on.
    // It is on by default after boot, so we leave it enabled on exit.
    furi_hal_power_enable_external_3_3v();

    FakeChipApp* app = fake_chip_app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    fake_chip_app_free(app);
    return 0;
}
