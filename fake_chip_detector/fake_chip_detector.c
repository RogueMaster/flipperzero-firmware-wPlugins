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
#include "onewire_worker.h"
#include "live_test.h"
#include "live_plugin.h"
#include "chip_db.h"
#include "i2c_notify.h"
#include "i2c_settings.h"
#include "report.h"

#define TAG "FakeChipDetector"

#define ANIM_PERIOD_MS 60 // ~16 fps, smooth enough and cheap

// Floor on the gap between two success chimes on a live test screen. Long
// enough that a reading sitting on its threshold cannot machine-gun the
// buzzer, short enough that genuinely doing the thing twice is heard twice.
#define LIVE_CHIME_MIN_GAP_MS 3000

typedef enum {
    FakeChipViewMenu,
    FakeChipViewWiring,
    FakeChipViewScan,
    FakeChipViewDetail,
    FakeChipViewLive,
    FakeChipViewSettings,
    FakeChipViewChips,
    FakeChipViewReport,
    FakeChipViewSaved,
    FakeChipViewOneWire,
    FakeChipViewSilent,
    FakeChipViewTests,
    FakeChipViewTestHelp,
    FakeChipViewSelftest,
    FakeChipViewAbout,
} FakeChipViewId;

typedef enum {
    MenuIndexWiring,
    MenuIndexScan,
    MenuIndexOneWire,
    MenuIndexTests,
    MenuIndexSelftest,
    MenuIndexSettings,
    MenuIndexChips,
    MenuIndexSaved,
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
    // The sweep is over and the serial line is being heard out. Its own flag
    // rather than a stage of `scanning`, because the progress bar is finished
    // and full by then and the screen has a different thing to say.
    bool listening;
    uint8_t listen_outcome; // UartListenOutcome, once the listen has ended
    UartListenResult listen;
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
    const LiveTest* test; // which module is running; NULL means nothing to run
    uint8_t addr; // where the scan found it, so no test has to search again
    LiveTestState state;
    uint32_t frame;
    // True when the test came off the SD card. Shown on screen, because a
    // built-in test was written against a datasheet and reviewed in this
    // repository, and one from the card is somebody else's code — anyone
    // reading a pass off this screen is entitled to know which it was.
    bool from_card;
} LiveViewModel;

typedef struct {
    uint16_t selected;
} ChipsViewModel;

#define SAVED_MAX      32
#define SAVED_NAME_LEN 32

typedef struct {
    OneWireScanResult res;
    uint8_t selected;
    bool busy;
    bool explain; // the "what this proves" panel is showing

    // Two fields rather than one, because a write that failed has to be as
    // loud as one that worked. A user who walks away believing a file exists
    // is worse off than one who was told plainly that it does not.
    char saved_name[SAVED_NAME_LEN]; // filename of the last report, on success
    bool save_failed; // the SD write did not go through
} OneWireViewModel;

typedef struct {
    // Allocated only while the screen is open: 32 filenames is a KB, and this
    // app has crashed users before by holding memory it was not using.
    char (*names)[SAVED_NAME_LEN];
    uint8_t count;
    uint16_t skipped; // matching files past SAVED_MAX, reported rather than hidden
    uint8_t selected;
} SavedViewModel;

typedef struct {
    // Same reason as SavedViewModel above, and more so: a full folder of
    // plugin descriptions is over two kilobytes, and this screen is open for
    // seconds at a time.
    LivePluginList* plugins;
    uint16_t selected;
    // Set when a manual launch found nothing at any of the test's addresses.
    // Shown in place of the offer line, then cleared by the next keypress.
    char message[LIVE_TEST_LINE_LEN];
} TestsViewModel;

// Things the worker thread needs the dispatcher thread to do. The worker may
// touch view models under their own lock, but navigation belongs to the thread
// that owns the event loop.
typedef enum {
    AppEventFixDone = 1,
} AppCustomEvent;

typedef struct {
    uint8_t selected; // which family of pad labels the user says they are on
    uint8_t level; // I2CPadLevel, as last settled by the meter
    uint32_t frame;
    uint8_t stage; // I2CFixStage; I2CFixIdle means the meter owns the screen
    // A pin that is only read at reset can be strapped with the one spare wire
    // and let go afterwards. One that has to be held cannot, and the screen
    // says that instead of pretending.
    bool needs_fifth_wire;
    // Frame the report was written on, or 0. A save nobody is told about is
    // the same as no save: the footer says so for a few seconds and then goes
    // back to explaining the pad.
    uint32_t saved_frame;
} SilentViewModel;

// The transport can be proved with one jumper and no sensor at all, and until
// this screen existed there was no way to ask for that from the device -- the
// only thing that ever called it was a plan. Somebody whose sensor is silent
// deserves to know whether the app's own serial path works before they conclude
// anything about the part in their hand.
typedef struct {
    uint8_t state; // I2CSelftestState
    bool running;
    char detail[I2C_SELFTEST_DETAIL_SIZE];
} SelftestViewModel;

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
    View* saved_view;
    View* onewire_view;
    View* silent_view;
    View* tests_view;
    View* test_help_view;
    View* selftest_view;
    TextBox* report_box;
    FuriString* report_text;
    VariableItemList* settings_list;
    Widget* about_widget;
    I2CWorker* worker;
    FuriThread* anim_thread;
    FuriThread* ow_thread;
    FuriThread* live_thread;
    volatile bool ow_abort;
    volatile bool live_stop;
    volatile bool anim_stop;
    I2CSettings settings;
    volatile FakeChipViewId current_view;
    bool live_chimed; // the success chime belongs to the run, not to each frame
    uint32_t live_chime_tick; // when it last played; 0 for not yet this run

    // Non-NULL while a test loaded from the SD card is on screen. The LiveTest
    // the worker is running points into this plugin's mapped memory, so it is
    // closed only after the thread has been joined.
    LivePluginHandle* live_plugin;

    // Where Back goes from a running live test: the screen it was started
    // from, which is either the scan verdict or the browser.
    FakeChipViewId live_return_to;
} FakeChipApp;

static void app_switch_view(FakeChipApp* app, FakeChipViewId view_id) {
    app->current_view = view_id;
    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}

// The saved-report filename is built here and read back here, so the two can
// never disagree. Nothing else may assume its shape.
#define REPORT_FILE_PREFIX "scan_"

static void report_filename_make(char* out, size_t out_size, const DateTime* dt) {
    snprintf(
        out,
        out_size,
        REPORT_FILE_PREFIX "%04u%02u%02u_%02u%02u%02u.txt",
        dt->year,
        dt->month,
        dt->day,
        dt->hour,
        dt->minute,
        dt->second);
}

// "scan_20260810_202233.txt" -> "2026-08-10 20:22". Falls back to the raw name
// rather than printing garbage if the file is not one of ours.
static void report_filename_label(const char* name, char* out, size_t out_size) {
    const size_t prefix = strlen(REPORT_FILE_PREFIX);
    if(strlen(name) < prefix + 15) {
        snprintf(out, out_size, "%s", name);
        return;
    }
    const char* n = name + prefix;
    snprintf(out, out_size, "%.4s-%.2s-%.2s %.2s:%.2s", n, n + 4, n + 6, n + 9, n + 11);
}

// A report a human reads aloud is a couple of kilobytes; this bounds what a
// damaged file can do to the heap.
#define REPORT_READ_MAX 8192

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
            for(uint8_t x = WIRE_X0; x <= mid - gap; x += 3)
                canvas_draw_dot(canvas, x, y);
            for(uint8_t x = WIRE_X1; x >= mid + gap; x -= 3)
                canvas_draw_dot(canvas, x, y);
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
            // Same reason as the answer below: what the last scan heard is not
            // evidence about this module. Clearing it here rather than trusting
            // the worker to overwrite it means a scan that is aborted before
            // the listen leaves nothing behind either.
            m->listening = false;
            m->listen_outcome = UartListenUnavailable;
            m->listen = (UartListenResult){0};
            m->status_msg[0] = '\0';
            // The question belongs to the module that was on the bus when it
            // was asked. Carrying the answer into the next scan silently skips
            // asking and hands the previous module's verdict to this one: a
            // genuine part accused of being "NOT YOURS", or worse, a part
            // nobody was asked about waved through as the real deal.
            m->answer = AnswerAsking;
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
static void draw_action_bar(Canvas* canvas, const char* ok_action, bool offer_save) {
    canvas_draw_box(canvas, 0, 55, 128, 9);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 62, ok_action);
    if(offer_save) {
        canvas_draw_str_aligned(canvas, 125, 62, AlignRight, AlignBottom, "save log");
        draw_right_key(canvas, 125 - canvas_string_width(canvas, "save log") - 9, 61);
    }
    canvas_set_color(canvas, ColorBlack);
}

// 24x24 thumbs-up, XBM (least significant bit leftmost). Hand-drawn: a
// verdict the user is happy about deserves more than a word.
#define THUMB_W 24
#define THUMB_H 24
static const uint8_t thumbs_up_bits[] = {
    0x00, 0x0F, 0x00, 0x80, 0x10, 0x00, 0x80, 0x10, 0x00, 0x80, 0x10, 0x00, 0x80, 0x10, 0x00,
    0x80, 0x10, 0x00, 0x80, 0x10, 0x00, 0xC0, 0xE0, 0xFF, 0x70, 0x00, 0x80, 0x18, 0x00, 0x80,
    0x0C, 0x00, 0x80, 0x04, 0xC0, 0xFF, 0x04, 0x00, 0x80, 0x04, 0x00, 0x80, 0x04, 0x00, 0x80,
    0x04, 0xC0, 0xFF, 0x04, 0x00, 0x80, 0x04, 0x00, 0x80, 0x04, 0x00, 0x80, 0x04, 0xC0, 0xFF,
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

// Bar for the ALL GOOD screen when the chip has a live test: the offer is the
// headline action, so it takes OK, and the hex stays behind Right.
static void draw_offer_bar(Canvas* canvas, const char* ok_action) {
    canvas_draw_box(canvas, 0, 55, 128, 9);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 62, "OK");
    canvas_draw_str(canvas, 20, 62, ok_action);
    canvas_draw_str_aligned(canvas, 124, 62, AlignRight, AlignBottom, "details");
    draw_right_key(canvas, 124 - canvas_string_width(canvas, "details") - 9, 61);
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
    canvas_draw_line(canvas, cx, cy, cx + (int8_t)(sinf(a) * 8.0f), cy - (int8_t)(cosf(a) * 8.0f));
    canvas_draw_disc(canvas, cx, cy, 1);
}

static void scan_draw_callback(Canvas* canvas, void* model) {
    ScanViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(m->scanning) {
        // Two seconds of an unmoving progress bar at 0x77 reads as a hang, and
        // a user who thinks the app has hung stops trusting the answer it
        // eventually gives. Say the thing instead.
        canvas_draw_str(canvas, 2, 12, m->listening ? "Nothing on I2C." : "Scanning bus...");
        draw_scan_spinner(canvas, 112, 20, m->frame);

        uint8_t span = I2C_SCAN_ADDR_LAST - I2C_SCAN_ADDR_FIRST;
        uint8_t done = m->progress_addr - I2C_SCAN_ADDR_FIRST;
        canvas_draw_frame(canvas, 4, 32, 92, 10);
        canvas_draw_box(canvas, 4, 32, (uint16_t)done * 92 / span, 10);

        canvas_set_font(canvas, FontSecondary);
        char buf[28];
        if(m->listening) {
            canvas_draw_str(canvas, 4, 52, "Listening on pin 16...");
        } else {
            snprintf(
                buf, sizeof(buf), "addr 0x%02X   found: %u", m->progress_addr, m->found_count);
            canvas_draw_str(canvas, 4, 52, buf);
        }
        return;
    }

    if(m->found_count == 0) {
        // "No devices found" is a verdict about the world. "No I2C answer" is
        // a statement about what happened, and the difference is not cosmetic:
        // someone read the old headline as a conclusion, pressed nothing else,
        // and handed a working sensor back to a courier.
        canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, "No I2C answer");
        canvas_set_font(canvas, FontSecondary);

        // Hints keyed to what the electrical check actually saw
        const char* l1;
        const char* l2;
        const char* l3;
        char heard[26];
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
            // The sweep covers 0x08-0x77, so a wrong address is not one of the
            // possibilities. A mode pin is: a part strapped to SPI or UART is
            // powered, its pull-ups still hold the bus high, and it will never
            // answer an address because in that state it does not have one.
            l1 = "Board has power and its";
            l2 = "pull-ups are there. Some";
            l3 = "chips boot with I2C off.";

            // Unless the listen actually heard the thing. Then this stops
            // being a list of possibilities and becomes a measurement, and it
            // is the strongest one this app can make without an ID register:
            // the part is powered, running, and talking on a bus that is not
            // I2C. Nobody should hand that back to a courier.
            if(m->listen_outcome == UartListenHeard) {
                if(m->listen.bytes) {
                    snprintf(
                        heard,
                        sizeof(heard),
                        "pin 16, at %lu baud.",
                        (unsigned long)m->listen.baud);
                    l1 = "Something IS talking on";
                    l2 = heard;
                    l3 = "That is a serial line.";
                } else {
                    // Framing errors and no clean bytes: the line is moving,
                    // but not at any rate that was tried. Claiming a rate here
                    // would be reporting the guess instead of the measurement.
                    l1 = "Pin 16 is switching, but";
                    l2 = "at no rate this tried.";
                    l3 = "Something is alive.";
                }
            }
            break;
        }
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignBottom, l1);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignBottom, l2);
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignBottom, l3);

        // The offer, not a full stop. The third line used to be the last item
        // in a list of bad possibilities with nothing attached to it.
        canvas_draw_box(canvas, 0, 55, 128, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 4, 62, "OK");
        canvas_draw_str(canvas, 20, 62, "rescan");
        canvas_draw_str_aligned(canvas, 124, 62, AlignRight, AlignBottom, "find out");
        draw_right_key(canvas, 124 - canvas_string_width(canvas, "find out") - 9, 61);
        canvas_set_color(canvas, ColorBlack);
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
            canvas_draw_str_aligned(
                canvas, 64, 40, AlignCenter, AlignBottom, "On the SD card, in");
            canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignBottom, "apps_data/");
            canvas_draw_str_aligned(
                canvas, 64, 58, AlignCenter, AlignBottom, "fake_chip_detector");
            return;
        }

        bool good = (m->answer == AnswerExpected) && chip_verdict_is_good(v);

        if(good) {
            // The whole point of the app, and the moment to be generous about
            // it: a thumb, a headline, and no hex anywhere in sight.
            //
            // Generous, but not louder than the evidence. A part with no ID
            // register was never verified — it turned up at the right address
            // and nothing more — so it does not get told it is the real deal.
            // Saying so here is the same rule the verdict screens follow, and
            // it is what makes the live test below worth pressing.
            bool proven = (v == VerdictGenuine);
            canvas_draw_xbm(canvas, 4, 7, THUMB_W, THUMB_H, thumbs_up_bits);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 36, 20, proven ? "ALL GOOD" : "IT ANSWERS");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 36, 30, proven ? "Real deal." : "No ID to check.");
            canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignBottom, name);

            // The ID said what it is; a live test says it works. Offered here
            // and only here, because this is the moment the answer is yes.
            const LiveTest* test = dev->ident.chip ? live_test_for_chip(dev->ident.chip->name) :
                                                     NULL;
            if(test) {
                canvas_draw_str_aligned(canvas, 64, 51, AlignCenter, AlignBottom, test->offer);
                draw_offer_bar(canvas, "live test");
            } else {
                if(kind) canvas_draw_str_aligned(canvas, 64, 51, AlignCenter, AlignBottom, kind);
                draw_hint_bar(canvas, "details", false);
            }
        } else {
            draw_verdict_icon(canvas, 12, 20, VerdictWrongChip);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 28, 16, "NOT YOURS");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 28, 26, "You were sold");
            // kind is NULL only when nothing in the database matched -- every
            // row has one, and the generator that builds SUPPORTED_CHIPS.md
            // fails the build if a row does not. So `name` here is the
            // "Unknown chip" placeholder rather than a part number: prose, and
            // it lowercases like prose.
            report_phrase_kind(buf, sizeof(buf), kind ? kind : name);
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

    draw_action_bar(canvas, "OK: details", true);
}

/* Files an already-built report in /ext/apps_data/fake_chip_detector/ and
 * hands back the name it chose. The document and the name it is filed under
 * are made from the same DateTime, so a report cannot be dated one minute in
 * its own text and another in the folder. Two kinds of report go through here
 * — the I2C scan and the 1-Wire bus — and only the writing is shared: the
 * documents themselves say different things and are built apart. */
static bool report_save_text(
    const FuriString* text,
    const DateTime* dt,
    char* out_name,
    size_t out_name_size) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = false;

    char name[SAVED_NAME_LEN];
    report_filename_make(name, sizeof(name), dt);
    if(out_name) snprintf(out_name, out_name_size, "%s", name);

    FuriString* path = furi_string_alloc_printf(APP_DATA_PATH("%s"), name);
    storage_common_resolve_path_and_ensure_app_directory(storage, path);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t len = furi_string_size(text);
        ok = storage_file_write(file, furi_string_get_cstr(text), len) == len;
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(path);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

/* Writes a snapshot of the scan results. Takes a copy rather than the live
 * model: SD writes can stall for seconds and must never run while the
 * view-model mutex is held. */
static bool scan_save_log(
    const I2CFoundDevice* found,
    uint8_t count,
    bool disputed,
    const SilentDiagnosis* silent,
    char* out_name,
    size_t out_name_size) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    FuriString* text = furi_string_alloc();
    report_build(text, found, count, disputed, &dt, silent);
    bool ok = report_save_text(text, &dt, out_name, out_name_size);
    furi_string_free(text);
    return ok;
}

static void app_show_report(FakeChipApp* app, bool disputed);
static void app_start_live_test(
    FakeChipApp* app,
    const LiveTest* test,
    uint8_t addr7,
    FakeChipViewId back_to);

// Snapshots the results and writes the report outside the model lock: SD
// writes can stall for seconds and must never hold up the GUI.
static void app_save_log(FakeChipApp* app, bool disputed, const SilentDiagnosis* silent) {
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
    bool saved = scan_save_log(snapshot, count, disputed, silent, name, sizeof(name));
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
    bool open_silent = false;
    const LiveTest* start_live = NULL;
    uint8_t live_addr = 0;

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
                        // On a clean verdict OK runs the live test if the part
                        // has one — that is the offer the screen is making.
                        // The report keeps Up either way.
                        const I2CFoundDevice* dev = &m->found[0];
                        if(m->answer == AnswerExpected &&
                           chip_verdict_is_good(dev->ident.verdict) && dev->ident.chip) {
                            start_live = live_test_for_chip(dev->ident.chip->name);
                            live_addr = dev->addr;
                        }
                        if(!start_live) {
                            show_report = true;
                            disputed = (m->answer == AnswerNotWhatIOrdered);
                        }
                    } else {
                        open_detail = true;
                    }
                    consumed = true;
                } else if(
                    event->key == InputKeyUp && m->found_count == 1 && m->answer != AnswerAsking &&
                    event->type == InputTypeShort) {
                    show_report = true;
                    disputed = (m->answer == AnswerNotWhatIOrdered);
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
                    event->key == InputKeyRight && m->found_count == 0 &&
                    event->type == InputTypeShort) {
                    open_silent = true; // the only screen that can still help
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

    if(start_live) app_start_live_test(app, start_live, live_addr, FakeChipViewScan);

    if(show_report) app_show_report(app, disputed);

    if(do_save) {
        app_save_log(app, disputed, NULL);
        i2c_notify_play(app->notifications, I2CNotifyNeutral);
    }

    if(rescan) app_start_scan(app);

    if(open_silent) app_switch_view(app, FakeChipViewSilent);

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

/* ---------------- Silent bus ---------------- */

// Where someone goes after a scan finds nothing and the wiring is fine. The
// question it answers is not "what chip is this" but "is it broken", and the
// two have different evidence: a part strapped to SPI or UART is healthy,
// powered and completely unable to answer an I2C address.
//
// It asks about the pad rather than the chip on purpose. The user arrived here
// because nothing identified itself, so a list of chip names invites a
// confident explanation of the wrong part - and the pad's label is printed on
// the board in their hand.
#define SILENT_ROWS 4

typedef struct {
    const char* labels; // what the board might silkscreen for this kind of pin
    const char* when_high;
    const char* when_low;
    bool explains_high; // this level is a reason the part cannot answer
    bool explains_low;
    bool want_high; // level this kind of pad needs for the part to speak I2C
    // Sampled once at reset and then kept. That is what makes a fix possible on
    // four wires: the pad only has to be held while the part restarts, so the
    // same wire can go back to carrying SDA afterwards. A pin that has to be
    // held the whole time cannot be fixed and helped at once with four wires,
    // and saying so is the honest answer.
    bool latched;
    // Which class of pin in chip_mode_pins these labels stand for, so the
    // screen can name the parts that have one. The user picked a silkscreen
    // label, not a chip, and (kind, alt, want_high) is what turns that label
    // back into a set of parts -- matching on the label strings themselves
    // would tie the database to the wording of a menu row.
    bool has_mode_class; // false for the address row: those pads pick no bus
    uint8_t kind; // ModePinKind
    uint8_t alt; // ModeAlt
} PadFamily;

// Named fields, not positions. Five of these are booleans in a row and one of
// them decides which way somebody is told to strap a pin: a transposition here
// would be invisible on the page and wrong on the bench.
static const PadFamily pad_families[SILENT_ROWS] = {
    // The SPI class: BME280, BMP280, most ST and ADI accelerometers. Every one
    // of them wants this pad high to stay on I2C. The Bosch parts latch it:
    // "the I2C interface is disabled until the next power-on-reset".
    {
        .labels = "CS  CSB  NSS",
        .when_high = "High = I2C. Not this.",
        .when_low = "Low picks SPI. This is it.",
        .explains_low = true,
        .want_high = true,
        .latched = true,
        .has_mode_class = true,
        .kind = ModePinProtocol,
        .alt = ModeAltSpi,
    },
    // Protocol select proper, which is the BNO055 case. Table 4-4: the pins are
    // read at reset and must not be left floating.
    {
        .labels = "PS0  PS1  SEL",
        .when_high = "High picks UART or SPI.",
        .when_low = "Low picks I2C. Not this.",
        .explains_high = true,
        .latched = true,
        .has_mode_class = true,
        .kind = ModePinProtocol,
        .alt = ModeAltUart,
    },
    // Not a protocol at all: the part is simply held off, and it stays off for
    // exactly as long as the pin is low. Nothing is latched, so there is no
    // moment where letting go is safe.
    {
        .labels = "XSHUT  RES  EN",
        .when_high = "High = enabled. Not this.",
        .when_low = "Low holds it in reset.",
        .explains_low = true,
        .want_high = true,
        .has_mode_class = true,
        .kind = ModePinEnable,
        .alt = ModeAltOff,
    },
    // Ruling something out is progress and should read as progress. No mode
    // class on purpose: an address pin picks no bus, so kind and alt are left
    // at zero and never read.
    {
        .labels = "AD0  ADR  SDO",
        .when_high = "Address only - all swept.",
        .when_low = "Address only - all swept.",
    },
};

// ~3 seconds at the 60ms tick. A save nobody is told about is the same as no
// save, and every screen here can write one.
static bool silent_saved_recently(const SilentViewModel* m) {
    return m->saved_frame && m->frame - m->saved_frame < 50;
}

// Is this level, on this kind of pad, a reason the part cannot answer?
static bool silent_pad_explains(const PadFamily* fam, uint8_t level) {
    if(level == I2CPadUnknown) return false; // nothing measured yet, nothing to conclude
    if(!fam->explains_high && !fam->explains_low) return false; // the address row
    // A mode pin left floating is not a grey area: BST-BNO055-DS000 says
    // outright that the protocol select pins may not be left floating, and
    // Bosch's pressure parts want CSB tied rather than left to drift. Whatever
    // such a pad settles on at power-up is luck, and luck is a reason to hold
    // it properly and try again.
    if(level == I2CPadFloating) return true;
    return (level == I2CPadHigh && fam->explains_high) ||
           (level == I2CPadLow && fam->explains_low);
}

// "Known: BMP280 BME280 +4". A rule with no example behind it is something to
// be taken on trust; a part number is something the reader can compare with
// the board in their hand. Empty when this build has no verified rows for the
// class, and empty is the right answer then -- a heading over nothing would
// read as "this tool knows of none", which is a different claim.
static void silent_known_parts(Canvas* canvas, const PadFamily* fam, char* out, size_t size) {
    out[0] = '\0';
    if(!fam->has_mode_class) return;

    // Measured against the font rather than counted in characters: these are
    // part numbers, all capitals and digits, and they run far wider per
    // character than the prose this footer normally carries. Room is kept for
    // the tail, because a list that quietly ran out of room reads as the whole
    // set -- and the whole set is what somebody would check their board
    // against.
    uint16_t budget = 126 - canvas_string_width(canvas, " +99");
    size_t used = 0;
    uint8_t hidden = 0;
    for(size_t i = 0; i < chip_mode_pin_count(); i++) {
        const ChipModePin* p = chip_mode_pin_get(i);
        if(!chip_mode_pin_matches(p, fam->kind, fam->alt, fam->want_high)) continue;
        if(!hidden) {
            snprintf(out + used, size - used, "%s%s", used ? " " : "Known: ", p->chip);
            if(canvas_string_width(canvas, out) <= budget) {
                used = strlen(out);
                continue;
            }
            out[used] = '\0'; // it did not fit; put the string back as it was
        }
        // Once one name has been dropped every later one is too, even where a
        // shorter one would have fitted. Skipping ahead to whatever fits would
        // print a list in an order that matches nothing.
        hidden++;
    }
    // used == 0 means even the first name was too wide, and then there is no
    // list for "+N" to be counting from.
    if(hidden && used) snprintf(out + used, size - used, " +%u", (unsigned)hidden);
}

// One instruction, one thing the app is waiting to see. Every stage but the
// first is left by a measurement, so the person can put the Flipper down and
// use both hands: nothing here needs a keypress to move on.
static void silent_draw_fix(Canvas* canvas, SilentViewModel* m) {
    const PadFamily* fam = &pad_families[m->selected];
    const char* title;
    const char* l1;
    const char* l2;
    const char* foot = NULL;
    bool spinner = true;

    switch(m->stage) {
    case I2CFixStrapping:
        title = fam->want_high ? "Holding the pad high" : "Holding the pad low";
        l1 = "Checking whether it";
        l2 = "moves.";
        break;
    case I2CFixPadHeld:
        // Not a failure to report apologetically: it is the answer. Somebody
        // chose this in copper and the buyer could not have known.
        title = "The board holds it";
        l1 = fam->want_high ? "This pad is tied low on" : "This pad is tied high on";
        l2 = "the board, not by you.";
        foot = "Left: save the report";
        spinner = false;
        break;
    case I2CFixWantPowerOff:
        title = "Pull the power wire";
        l1 = "Out of the sensor. Mode";
        l2 = "pins are read at startup.";
        foot = "Still powered.";
        break;
    case I2CFixWantPowerOn:
        title = "Power is off";
        l1 = "Plug it back in. The pad";
        l2 = "is held for you.";
        foot = "Waiting.";
        break;
    default: // I2CFixWantWire, and I2CFixDone for the frame before the rescan
        title = "Put the wire back";
        l1 = "On the sensor's SDA pad,";
        l2 = "Flipper end in pin 15.";
        foot = "Waiting for the bus.";
        break;
    }

    if(silent_saved_recently(m)) foot = "Saved to the SD card.";

    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignBottom, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignBottom, l1);
    canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignBottom, l2);
    if(spinner) draw_scan_spinner(canvas, 64, 48, m->frame);
    if(foot) {
        canvas_draw_box(canvas, 0, 55, 128, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, foot);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void silent_draw_callback(Canvas* canvas, void* model) {
    SilentViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(m->needs_fifth_wire) {
        // The one case four wires cannot solve, said plainly rather than
        // offered as a fix that quietly does nothing.
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignBottom, "Needs a fifth wire");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 27, AlignCenter, AlignBottom, "This pin has to stay high");
        canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignBottom, "the whole time, so it");
        canvas_draw_str_aligned(
            canvas, 64, 47, AlignCenter, AlignBottom, "cannot also carry SDA.");
        canvas_draw_box(canvas, 0, 55, 128, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas,
            64,
            62,
            AlignCenter,
            AlignBottom,
            silent_saved_recently(m) ? "Saved to the SD card." : "Left: save the report");
        canvas_set_color(canvas, ColorBlack);
        return;
    }

    if(m->stage != I2CFixIdle) {
        silent_draw_fix(canvas, m);
        return;
    }

    const PadFamily* fam = &pad_families[m->selected];
    bool explained = silent_pad_explains(fam, m->level);

    // The meter needs four agreeing reads before it will say anything, and
    // until then it says exactly that. Defaulting the headline to FLOATING
    // would have announced a level nothing had measured -- and FLOATING is the
    // one level that offers the fix on every mode family, so it would have
    // offered to act on it too.
    const char* title = "Reading the pad...";
    if(m->level == I2CPadFloating) title = "Pad reads FLOATING";
    if(m->level == I2CPadHigh) title = "Pad reads HIGH";
    if(m->level == I2CPadLow) title = "Pad reads LOW";
    canvas_draw_str_aligned(canvas, 2, 10, AlignLeft, AlignBottom, title);

    canvas_set_font(canvas, FontSecondary);
    // The fix is offered only once the reading and the chosen pad actually
    // form a cause; before that there is nothing to fix and saying so would be
    // a guess. Left saves either way -- Down belongs to the list, and the hint
    // yields to the stronger action rather than the key going away.
    canvas_draw_str_aligned(
        canvas, 126, 10, AlignRight, AlignBottom, explained ? "OK fix" : "< save");
    for(uint8_t row = 0; row < SILENT_ROWS; row++) {
        uint8_t y = 22 + row * 10;
        bool sel = (row == m->selected);
        if(sel) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 4, y, pad_families[row].labels);
        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    // The footer is the whole point: it turns a bare level into a reason, and
    // it changes as the cursor moves so nobody has to hold a lookup table in
    // their head at a counter.
    canvas_draw_box(canvas, 0, 55, 128, 9);
    canvas_set_color(canvas, ColorWhite);
    // Wide enough that the pixel budget always runs out first: no glyph in
    // this font is narrower than two pixels, so 126 pixels cannot hold 64
    // characters, and a name can never be cut in half by the buffer.
    char known[64];
    const char* foot;
    if(silent_saved_recently(m)) {
        foot = "Saved to the SD card.";
    } else if(m->level == I2CPadUnknown) {
        // Falling through to the high/low line here would have printed what a
        // LOW reading means, on a pad nothing had read yet.
        foot = "Measuring...";
    } else if(m->level == I2CPadFloating) {
        // Alternating, because the likeliest cause is not the pad at all: the
        // wire got moved at the Flipper end instead of the sensor end.
        foot = (m->frame / 24) % 2 ? "Wire still in pin 15?" : "Nothing drives this pad.";
    } else {
        foot = m->level == I2CPadHigh ? fam->when_high : fam->when_low;
        // The parts get a turn only once the reading and the pad agree that
        // something is wrong. Naming the SPI-selecting parts under "High =
        // I2C. Not this." would have the footer argue with itself.
        //
        // Alternating rather than replacing: the rule is the answer and has to
        // come back round. Somebody holding a module that is on neither list
        // still needs to be told what the level means.
        if(explained && (m->frame / 24) % 2) {
            silent_known_parts(canvas, fam, known, sizeof(known));
            if(known[0]) foot = known;
        }
    }
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, foot);
    canvas_set_color(canvas, ColorBlack);
}

static void silent_enter_callback(void* context) {
    FakeChipApp* app = context;
    app->current_view = FakeChipViewSilent;
    with_view_model(
        app->silent_view,
        SilentViewModel * m,
        {
            m->stage = I2CFixIdle;
            m->needs_fifth_wire = false;
            m->saved_frame = 0;
            m->level = I2CPadUnknown; // the meter fills this in, ~200ms from now
        },
        true);
    i2c_worker_pad_watch_start(app->worker);
}

static bool silent_input_callback(InputEvent* event, void* context) {
    FakeChipApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = false;
    bool start_fix = false;
    bool want_high = false;
    bool back_to_meter = false;
    bool do_save = false;
    SilentDiagnosis diag = {0};
    with_view_model(
        app->silent_view,
        SilentViewModel * m,
        {
            bool in_fix = m->stage != I2CFixIdle || m->needs_fifth_wire;
            if(event->key == InputKeyLeft) {
                // Saving is allowed from every one of these screens on purpose:
                // "the board ties this pad low" is exactly the sentence someone
                // needs in writing at a parcel counter.
                diag.pad_measured = m->level != I2CPadUnknown;
                diag.pad_level = m->level;
                diag.pad_labels = pad_families[m->selected].labels;
                diag.pad_held = m->stage == I2CFixPadHeld;
                diag.pad_wanted_high = pad_families[m->selected].want_high;
                diag.pad_mode_known = pad_families[m->selected].has_mode_class;
                diag.pad_mode_kind = pad_families[m->selected].kind;
                diag.pad_mode_alt = pad_families[m->selected].alt;
                m->saved_frame = m->frame ? m->frame : 1;
                do_save = true;
                consumed = true;
            } else if(event->key == InputKeyBack && in_fix) {
                m->stage = I2CFixIdle;
                m->needs_fifth_wire = false;
                back_to_meter = true;
                consumed = true;
            } else if(in_fix) {
                consumed = true; // the run advances on measurements, not keys
            } else if(event->key == InputKeyUp && m->selected > 0) {
                m->selected--;
                consumed = true;
            } else if(event->key == InputKeyDown && m->selected + 1 < SILENT_ROWS) {
                m->selected++;
                consumed = true;
            } else if(event->key == InputKeyOk) {
                const PadFamily* fam = &pad_families[m->selected];
                bool explained = silent_pad_explains(fam, m->level);
                if(explained && fam->latched) {
                    m->stage = I2CFixStrapping;
                    want_high = fam->want_high;
                    start_fix = true;
                    consumed = true;
                } else if(explained) {
                    m->needs_fifth_wire = true;
                    consumed = true;
                }
            }
        },
        consumed);

    // Outside the model lock, like every other input handler here: the fix run
    // waits on a person's hands and an SD write can stall for seconds.
    if(start_fix) i2c_worker_fix_start(app->worker, want_high);
    if(back_to_meter) {
        i2c_worker_fix_stop(app->worker);
        i2c_worker_pad_watch_start(app->worker);
    }

    if(do_save) {
        i2c_worker_get_bus(app->worker, &diag.bus);
        // Straight from the worker rather than from the scan view model: this
        // is the one measurement in the document that the app made on its own,
        // without being asked, and it should not depend on a screen the user
        // may never have looked at having been updated.
        diag.listen_outcome = (uint8_t)i2c_worker_get_listen(app->worker, &diag.listen);
        app_save_log(app, false, &diag);
        i2c_notify_play(app->notifications, I2CNotifyNeutral);
    }
    return consumed;
}

static void silent_exit_callback(void* context) {
    FakeChipApp* app = context;
    // Guarded, never unconditional: the dispatcher runs this after the next
    // view has already claimed the slot, so an unconditional assignment undoes
    // the switch that is happening right now.
    if(app->current_view == FakeChipViewSilent) app->current_view = FakeChipViewScan;
    i2c_worker_pad_watch_stop(app->worker);
    i2c_worker_fix_stop(app->worker); // releases the strap and the rail with it
}

static void worker_event_callback(I2CWorkerEvent event, void* context) {
    FakeChipApp* app = context;

    if(event == I2CWorkerEventScanProgress || event == I2CWorkerEventScanDone) {
        bool done = (event == I2CWorkerEventScanDone);
        uint8_t count = 0;
        bool any_genuine = false, any_bad = false;
        I2CBusCheck bus;
        i2c_worker_get_bus(app->worker, &bus);
        bool listening = i2c_worker_is_listening(app->worker);
        UartListenResult listen;
        UartListenOutcome listen_outcome = i2c_worker_get_listen(app->worker, &listen);

        with_view_model(
            app->scan_view,
            ScanViewModel * m,
            {
                m->progress_addr = i2c_worker_get_progress(app->worker);
                m->found_count = i2c_worker_get_found(app->worker, m->found, I2C_SCAN_MAX_FOUND);
                m->bus = bus;
                m->listening = listening;
                m->listen_outcome = (uint8_t)listen_outcome;
                m->listen = listen;
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

            if(app->settings.autosave && count > 0) app_save_log(app, false, NULL);
        }
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
                bool wrong = bus.shorted || bus.stray_pin || bus.health == I2CBusStuckLow;
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
    } else if(event == I2CWorkerEventSelftestDone) {
        char detail[I2C_SELFTEST_DETAIL_SIZE];
        I2CSelftestState state = i2c_worker_get_selftest(app->worker, detail, sizeof(detail));
        with_view_model(
            app->selftest_view,
            SelftestViewModel * m,
            {
                m->state = (uint8_t)state;
                m->running = false;
                snprintf(m->detail, sizeof(m->detail), "%s", detail);
            },
            true);
    } else if(event == I2CWorkerEventPadUpdate) {
        uint8_t level = (uint8_t)i2c_worker_get_pad(app->worker);
        with_view_model(app->silent_view, SilentViewModel * m, { m->level = level; }, true);
    } else if(event == I2CWorkerEventFixUpdate) {
        uint8_t stage = (uint8_t)i2c_worker_get_fix_stage(app->worker);
        with_view_model(app->silent_view, SilentViewModel * m, { m->stage = stage; }, true);
        // Hand the rest to the dispatcher thread. This callback runs on the
        // worker, and switching views from here would drive the GUI from two
        // threads at once; the custom-event queue is the seam that exists for
        // exactly this.
        if(stage == I2CFixDone) {
            view_dispatcher_send_custom_event(app->view_dispatcher, AppEventFixDone);
        }
    }
}

static bool app_custom_event_callback(void* context, uint32_t event) {
    FakeChipApp* app = context;
    if(event == AppEventFixDone) {
        // Straight into a rescan with no further keypress. Whoever pressed OK
        // has just finished moving a wire with one hand and holding the sensor
        // with the other; asking for one more press is asking them to let go.
        app_start_scan(app);
        app_switch_view(app, FakeChipViewScan);
        return true;
    }
    return false;
}

/* ---------------- Wiring enter/exit ---------------- */

static void wiring_enter_callback(void* context) {
    FakeChipApp* app = context;
    app->current_view = FakeChipViewWiring;
    with_view_model(
        app->wiring_view,
        WiringViewModel * m,
        {
            m->frame = 0;
            m->sensor_seen = false;
            m->bus = (I2CBusCheck){0};
            for(uint8_t i = 0; i < 4; i++)
                m->gap[i] = WIRE_GAP_MAX;
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
static void app_present_report(FakeChipApp* app, ViewNavigationCallback back_to) {
    text_box_reset(app->report_box);
    text_box_set_font(app->report_box, TextBoxFontText);
    text_box_set_text(app->report_box, furi_string_get_cstr(app->report_text));
    view_set_previous_callback(text_box_get_view(app->report_box), back_to);
    app_switch_view(app, FakeChipViewReport);
}

static uint32_t nav_to_saved(void* context) {
    UNUSED(context);
    return FakeChipViewSaved;
}

static uint32_t nav_to_scan(void* context) {
    UNUSED(context);
    return FakeChipViewScan;
}

/* ---------------- Live tests ---------------- */

// Nothing here knows about any particular chip. What a test measures — and
// how it draws that, if text will not do — lives in live_<part>.c; see
// live_test.h for the contract and how to add one.

// Steps a test can ask to have counted out as boxes. Eleven of them at nine
// pixels fill the hundred-pixel strip the bar has; nothing wider fits, and a
// test asking for more is asking for something that cannot be drawn.
#define LIVE_PROGRESS_MAX_CELLS 11

// The screen for every phase where there is nothing to measure yet. Shared on
// purpose: "warming up" and "it dropped off" should look the same whichever
// part is being tested, so only the readings get a custom picture.
static void live_draw_generic(Canvas* canvas, const LiveViewModel* m) {
    const LiveTestState* st = &m->state;
    bool measuring = (st->phase == LiveTestPhaseRunning || st->phase == LiveTestPhasePassed);

    canvas_set_font(canvas, FontPrimary);
    // Two different failures, two different headlines. "Dropped off" sends the
    // user to the wiring; "wrong chip" tells them the wiring is fine and the
    // module is not what the test is for. Saying the first when it is the
    // second is how somebody ends up reseating a perfectly good jumper.
    const char* title =
        (st->phase == LiveTestPhaseLost)      ? "Sensor dropped off!" :
        (st->phase == LiveTestPhaseWrongChip) ? "Wrong chip!" :
                                                (m->test ? m->test->title : "Live test");
    canvas_draw_str_aligned(canvas, 64, 13, AlignCenter, AlignBottom, title);

    if(m->from_card) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 12, "SD");
        canvas_set_font(canvas, FontPrimary);
    }

    // "It passed" is the app's job to say, not each test's. Drawing it here
    // means one tick in one place, identical for every module, and leaves the
    // test's own lines free to keep showing the reading that earned it.
    if(st->phase == LiveTestPhasePassed) {
        canvas_draw_line(canvas, 118, 9, 120, 12);
        canvas_draw_line(canvas, 118, 10, 120, 13);
        canvas_draw_line(canvas, 120, 12, 125, 5);
        canvas_draw_line(canvas, 120, 13, 125, 6);
    }

    uint8_t y = 26;
    if(measuring && st->heading[0]) {
        // Number and unit are centred as one block, so the digits do not
        // shuffle sideways every time the reading gains or loses a character.
        // Each width is measured with its own font already selected —
        // canvas_string_width answers for whatever font is current, and asking
        // in the wrong one lands the unit on top of the number.
        canvas_set_font(canvas, FontBigNumbers);
        uint8_t num_w = canvas_string_width(canvas, st->heading);
        uint8_t unit_w = 0;
        if(st->unit[0]) {
            canvas_set_font(canvas, FontSecondary);
            unit_w = canvas_string_width(canvas, st->unit) + 3;
        }
        uint8_t x = (num_w + unit_w >= 128) ? 0 : (uint8_t)(64 - (num_w + unit_w) / 2);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, x, 36, st->heading);
        if(st->unit[0]) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, x + num_w + 3, 36, st->unit);
        }
        y = 46;
    }

    // A bar sits in the bottom strip of the screen rather than flowing after
    // the text, because the text is what decides whether it appears at all:
    // laid out in sequence, a heading and two lines left y past the bottom and
    // the bar was silently never drawn — which is exactly the shape the AHT
    // test publishes on every poll. Reserving the strip costs a pixel of line
    // spacing and keeps both.
    const bool has_bar = measuring && (st->bar_max || st->progress_max);
    const uint8_t bar_y = 57; // a 7-pixel bar ends on row 63, the last one
    const uint8_t line_limit = has_bar ? (uint8_t)(bar_y - 2) : 62;
    const uint8_t line_step = has_bar ? 9 : 10;

    canvas_set_font(canvas, FontSecondary);
    for(size_t i = 0; i < LIVE_TEST_LINES && y <= line_limit; i++) {
        if(!st->lines[i][0]) continue;
        canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignBottom, st->lines[i]);
        y += line_step;
    }

    if(st->phase == LiveTestPhaseStarting && y + 8 <= 62) {
        // A sweep, not a percentage: the wait is a fixed settle time, and a
        // fake progress bar that claims to know how far along it is would be
        // exactly the kind of made-up number this app refuses to show.
        uint8_t w = (uint8_t)((m->frame % 20) * 100 / 20);
        canvas_draw_frame(canvas, 14, y, 100, 8);
        canvas_draw_box(canvas, 14, y, w, 8);
    } else if((st->phase == LiveTestPhaseLost || st->phase == LiveTestPhaseWrongChip) && y <= 62) {
        // True for both: the test's outer loop keeps re-checking, so swapping
        // the module or pushing the wire back in picks up without leaving.
        canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignBottom, "Retrying...");
    } else if(measuring && st->bar_max) {
        uint8_t fill = st->bar > st->bar_max ? st->bar_max : st->bar;
        canvas_draw_frame(canvas, 14, bar_y, 100, 7);
        canvas_draw_box(canvas, 14, bar_y, (uint8_t)((uint32_t)100 * fill / st->bar_max), 7);
    } else if(measuring && st->progress_max) {
        uint8_t bw = st->progress_max * 9 - 2;
        uint8_t bx = 64 - bw / 2;
        for(uint8_t i = 0; i < st->progress_max; i++) {
            if(i < st->progress) {
                canvas_draw_box(canvas, bx + i * 9, bar_y, 7, 7);
            } else {
                canvas_draw_frame(canvas, bx + i * 9, bar_y, 7, 7);
            }
        }
    }
}

static void live_draw_callback(Canvas* canvas, void* model) {
    LiveViewModel* m = model;
    canvas_clear(canvas);

    bool measuring =
        (m->state.phase == LiveTestPhaseRunning || m->state.phase == LiveTestPhasePassed);
    if(measuring && m->test && m->test->draw) {
        m->test->draw(canvas, &m->state, m->frame);
    } else {
        live_draw_generic(canvas, m);
    }
}

// Called from the test's own thread. Copies the state under the model lock and
// owns the one thing a test must not decide for itself: when to make a noise.
static void live_publish(void* ctx, const LiveTestState* state) {
    FakeChipApp* app = ctx;
    with_view_model(
        app->live_view,
        LiveViewModel * m,
        {
            m->state = *state;
            // The test filled these, and a test can be somebody else's .fal off
            // the SD card. One that writes its full buffer width leaves no
            // terminator, and the draw callback then walks out of the array and
            // off the end of this model — on a part with no MMU that is a hard
            // fault mid-measurement, not a garbled line. The last byte of each
            // is the app's, not the test's.
            m->state.heading[LIVE_TEST_HEADING_LEN - 1] = '\0';
            m->state.unit[LIVE_TEST_UNIT_LEN - 1] = '\0';
            for(size_t i = 0; i < LIVE_TEST_LINES; i++) {
                m->state.lines[i][LIVE_TEST_LINE_LEN - 1] = '\0';
            }
            // Same reason, in arithmetic rather than in bytes. A step count the
            // strip cannot hold makes `progress_max * 9 - 2` wrap the width it
            // is measured in and sends the draw loop round two hundred times
            // for a row of boxes off the side of the screen.
            if(m->state.progress_max > LIVE_PROGRESS_MAX_CELLS) {
                m->state.progress_max = LIVE_PROGRESS_MAX_CELLS;
            }
            if(m->state.progress > m->state.progress_max) {
                m->state.progress = m->state.progress_max;
            }
        },
        true);

    bool succeeded = (state->phase == LiveTestPhasePassed) ||
                     (state->progress_max && state->progress >= state->progress_max);
    if(succeeded && !app->live_chimed) {
        // Rate limited, and not as a nicety. Re-arming on the falling edge is
        // what makes a second success audible, but it also means a reading
        // flapping across the threshold plays the chime at whatever rate the
        // test polls at — which is exactly what a wrong chip returning garbage
        // did, twice a second, until it had to be unplugged. A test can come
        // off somebody else's SD card, so the buzzer is the app's to bound.
        uint32_t now = furi_get_tick();
        if(!app->live_chime_tick ||
           now - app->live_chime_tick >= furi_ms_to_ticks(LIVE_CHIME_MIN_GAP_MS)) {
            app->live_chime_tick = now;
            i2c_notify_play(app->notifications, I2CNotifyCalibrated);
            // Latched only once it has actually been heard, so the chime fires
            // on the transition rather than on every poll. Latching a
            // suppressed one instead would spend the success on silence: the
            // rate limit would swallow it and the latch would stop it ever
            // being retried, leaving a tick on screen with no sound at all.
            app->live_chimed = true;
        }
    } else if(!succeeded) {
        app->live_chimed = false; // re-arm when the part falls back out
    }
}

static int32_t live_thread_worker(void* context) {
    FakeChipApp* app = context;
    const LiveTest* test = NULL;
    uint8_t addr = 0;
    with_view_model(
        app->live_view,
        LiveViewModel * m,
        {
            test = m->test;
            addr = m->addr;
        },
        false);

    if(test && test->run) {
        // Built on this thread's stack and handed over by pointer. The test
        // may be a module compiled into this app or one loaded from the SD
        // card, and this struct is the entire difference between them —
        // neither reaches for a symbol of ours by name.
        const LiveTestEnv env = {
            .addr7 = addr,
            .stop = &app->live_stop,
            .publish = live_publish,
            .ctx = app,
            .i2c = live_test_i2c(),
        };
        test->run(&env);
    }
    return 0;
}

// `back_to` is passed rather than read from app->current_view, which only
// tracks forward navigation: a Back keypress goes through the view's previous
// callback and never updates it. Inferring the origin from it sent anyone who
// entered the browser by pressing Back to the scan verdict instead.
static void app_start_live_test(
    FakeChipApp* app,
    const LiveTest* test,
    uint8_t addr7,
    FakeChipViewId back_to) {
    app->live_return_to = back_to;
    with_view_model(
        app->live_view,
        LiveViewModel * m,
        {
            m->test = test;
            m->addr = addr7;
            m->from_card = (app->live_plugin != NULL);
        },
        false);
    app_switch_view(app, FakeChipViewLive);
}

static void live_enter_callback(void* context) {
    FakeChipApp* app = context;
    app->current_view = FakeChipViewLive;
    app->live_chimed = false;
    app->live_chime_tick = 0;
    with_view_model(
        app->live_view,
        LiveViewModel * m,
        {
            memset(&m->state, 0, sizeof(m->state));
            m->frame = 0;
        },
        true);

    // The thread exists only while the screen does. A live test talks to the
    // chip continuously, and leaving one running behind a menu would keep the
    // sensor powered up and the I2C bus busy for no reason.
    app->live_stop = false;
    app->live_thread = furi_thread_alloc_ex("FakeChipLive", 2048, live_thread_worker, app);
    furi_thread_start(app->live_thread);
}

// How long a test gets to notice the stop flag before the app says out loud
// that it is the test's fault. Every shipped test slices its delays, so the
// longest honest wait is one slice; two seconds is far past that.
#define LIVE_STOP_GRACE_MS 2000

static void live_exit_callback(void* context) {
    FakeChipApp* app = context;
    app->live_stop = true;
    if(app->live_thread) {
        // This runs on the dispatcher's thread, so the wait is felt as the
        // whole app stopping — and the code being waited for can be somebody
        // else's .fal. Giving up on the join is not the way out of that: the
        // worker holds a pointer to this view model and, for a card test, is
        // executing inside a mapping the lines below are about to unmap.
        // Returning early would trade a stall the user can see for a write into
        // freed memory they cannot, which is the one failure this app must not
        // produce. So the join stays.
        //
        // What is said about it cannot be said on the screen. The dispatcher
        // runs its views on an event loop, and this thread is that loop: a
        // model committed with update=true only queues a repaint for a consumer
        // that is standing right here, so nothing would be drawn — and the
        // animation thread is filling that same queue at every tick, so putting
        // one more message on it from this side risks blocking forever on a
        // queue nobody can drain. A stall would become a hang.
        //
        // The notification service has its own thread and keeps draining, so
        // the attention chirp is the one signal that still gets out. It says
        // "this is the app, not your imagination" and nothing more; the honest
        // description of what went wrong belongs in LIVE_TESTS.md, where it is
        // addressed to whoever wrote the test.
        uint32_t waited = 0;
        bool complained = false;
        while(furi_thread_get_state(app->live_thread) != FuriThreadStateStopped) {
            if(!complained && waited >= LIVE_STOP_GRACE_MS) {
                complained = true;
                i2c_notify_play(app->notifications, I2CNotifyAttention);
            }
            furi_delay_ms(20);
            waited += 20;
        }
        furi_thread_join(app->live_thread);
        furi_thread_free(app->live_thread);
        app->live_thread = NULL;
    }

    // Strictly after the join, and the order of these two lines is the whole
    // point. The worker is not the only thread holding a pointer into the
    // plugin: the GUI thread draws m->test->title on every frame, and
    // m->test->draw if the plugin supplied one. Joining says nothing about
    // that. Clearing m->test under the model lock does — taking the lock waits
    // out any draw already in progress, and once it is NULL no later draw can
    // follow the pointer. Only then is it safe to unmap.
    if(app->live_plugin) {
        with_view_model(app->live_view, LiveViewModel * m, { m->test = NULL; }, false);
        live_plugin_close(app->live_plugin);
        app->live_plugin = NULL;
    }
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
        idx ? &sequence_display_backlight_enforce_on : &sequence_display_backlight_enforce_auto);
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

// The list is every I2C chip, then every 1-Wire family. Both are things the
// app can name off the bus, so leaving either out of "what do you know?"
// under-reports it — a user with a DS18B20 who does not find it here concludes
// the app cannot identify it, when it can.
static size_t chips_total(void) {
    return chip_db_count() + onewire_family_count();
}

// One row, whichever table it comes from; every out parameter may be NULL.
// is_onewire also decides the heading, because the two buses do not carry the
// same promise: an I2C part can be called GENUINE off its ID register, a
// 1-Wire ROM can be replayed by any microcontroller and never is.
static bool chips_row(size_t idx, const char** name, const char** kind, bool* is_onewire) {
    const char* row_name;
    const char* row_kind;
    bool row_onewire;

    size_t i2c_count = chip_db_count();
    if(idx < i2c_count) {
        const ChipEntry* chip = chip_db_get(idx);
        if(!chip) return false;
        row_name = chip->name;
        row_kind = chip->kind;
        row_onewire = false;
    } else {
        const OneWireFamily* family = onewire_family_get(idx - i2c_count);
        if(!family) return false;
        row_name = family->name;
        row_kind = family->kind;
        row_onewire = true;
    }

    if(name) *name = row_name;
    if(kind) *kind = row_kind;
    if(is_onewire) *is_onewire = row_onewire;
    return true;
}

// Answers "what does this thing actually know?", and doubles as the place
// where every name and description is shown at full width — if one of them
// were too long for the screen, it would be obvious here.
static void chips_draw_callback(Canvas* canvas, void* model) {
    ChipsViewModel* m = model;
    size_t total = chips_total();
    canvas_clear(canvas);

    const char* sel_name = NULL;
    const char* sel_kind = NULL;
    bool sel_onewire = false;
    bool have_sel = chips_row(m->selected, &sel_name, &sel_kind, &sel_onewire);

    char buf[24];
    canvas_set_font(canvas, FontPrimary);
    // The heading follows the selection rather than sitting still: scrolling
    // off the end of the I2C table into the 1-Wire families is otherwise
    // invisible, and the two are not interchangeable.
    canvas_draw_str(canvas, 2, 10, sel_onewire ? "1-Wire parts" : "Known chips");
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%u/%u", (unsigned)m->selected + 1, (unsigned)total);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, buf);

    uint16_t first = 0;
    if(m->selected >= CHIPS_LIST_ROWS) first = m->selected - CHIPS_LIST_ROWS + 1;

    for(uint8_t row = 0; row < CHIPS_LIST_ROWS; row++) {
        size_t idx = first + row;
        const char* name = NULL;
        if(idx >= total || !chips_row(idx, &name, NULL, NULL)) break;
        uint8_t y = 22 + row * 10;
        bool sel = (idx == m->selected);
        if(sel) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 4, y, name);
        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    // The description gets a line of its own. Packing it beside the name made
    // the two collide as soon as either was long.
    if(have_sel) {
        canvas_draw_box(canvas, 0, 55, 128, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, sel_kind);
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
            size_t total = chips_total();
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

/* ---------------- Live test browser ---------------- */

#define TESTS_LIST_ROWS 4

// The list is the built-in tests, then whatever was found on the card, then
// one synthetic row that opens the instructions. Keeping the help row in the
// list rather than on a hint bar costs no screen furniture and is the first
// thing a user scrolls to the bottom and finds.
static size_t tests_total(const TestsViewModel* m) {
    return live_test_count() + (m->plugins ? m->plugins->count : 0) + 1;
}

static bool tests_row_is_help(const TestsViewModel* m, size_t index) {
    return index + 1 == tests_total(m);
}

// NULL for the help row or an out-of-range index.
static const LivePluginInfo* tests_row_plugin(const TestsViewModel* m, size_t index) {
    if(!m->plugins) return NULL;
    if(index < live_test_count()) return NULL;
    size_t slot = index - live_test_count();
    if(slot >= m->plugins->count) return NULL;
    return &m->plugins->items[slot];
}

static void
    tests_row_label(const TestsViewModel* m, size_t index, const char** name, const char** note) {
    const LivePluginInfo* plugin = tests_row_plugin(m, index);
    if(plugin) {
        // A plugin that failed to load still gets a row. Hiding it would leave
        // the user staring at a folder whose contents do not appear, with no
        // way to find out why.
        *name = plugin->status == LivePluginOk ? plugin->chip : plugin->file;
        *note = plugin->status == LivePluginOk ? plugin->offer :
                                                 live_plugin_status_text(plugin->status);
        return;
    }
    const LiveTest* test = live_test_get(index);
    *name = test ? test->chip : "?";
    *note = test ? test->offer : "";
}

static void tests_draw_callback(Canvas* canvas, void* model) {
    TestsViewModel* m = model;
    size_t total = tests_total(m);
    canvas_clear(canvas);

    char buf[24];
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Live tests");
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%u on card", (unsigned)(m->plugins ? m->plugins->count : 0));
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, buf);

    uint16_t first = 0;
    if(m->selected >= TESTS_LIST_ROWS) first = m->selected - TESTS_LIST_ROWS + 1;

    for(uint8_t row = 0; row < TESTS_LIST_ROWS; row++) {
        size_t idx = first + row;
        if(idx >= total) break;
        uint8_t y = 22 + row * 10;
        bool sel = (idx == m->selected);
        if(sel) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }

        if(tests_row_is_help(m, idx)) {
            canvas_draw_str(canvas, 4, y, "Add your own...");
        } else {
            const char *name = NULL, *note = NULL;
            tests_row_label(m, idx, &name, &note);
            canvas_draw_str(canvas, 4, y, name);

            // Where a test came from is not a detail. A built-in test was
            // written against a datasheet and reviewed here; one from the card
            // is somebody else's code, and the person reading a PASS off this
            // screen deserves to know which they are looking at.
            if(tests_row_plugin(m, idx)) {
                canvas_draw_str_aligned(canvas, 124, y, AlignRight, AlignBottom, "SD");
            }
        }
        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    canvas_draw_box(canvas, 0, 55, 128, 9);
    canvas_set_color(canvas, ColorWhite);
    const char* footer;
    if(m->message[0]) {
        footer = m->message;
    } else if(tests_row_is_help(m, m->selected)) {
        footer = "Write one, drop it in";
    } else {
        const char *name = NULL, *note = NULL;
        tests_row_label(m, m->selected, &name, &note);
        footer = note;
    }
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, footer);
    canvas_set_color(canvas, ColorBlack);
}

// Finds which of a test's declared addresses is actually answering. Returns 0
// when none does — deliberately, rather than falling back to the first one:
// running a test against an address with nothing on it would mean writing
// configuration to whatever else happens to be there.
static uint8_t tests_probe(const uint8_t* addrs) {
    for(size_t i = 0; i < LIVE_TEST_MAX_ADDRS; i++) {
        if(addrs[i] == LIVE_TEST_ADDR_NONE) break;
        if(i2c_worker_device_ready(addrs[i], I2C_PROBE_TIMEOUT_MS)) return addrs[i];
    }
    return 0;
}

static void tests_describe_addrs(char* out, size_t len, const uint8_t* addrs) {
    size_t used = 0;
    out[0] = '\0';
    for(size_t i = 0; i < LIVE_TEST_MAX_ADDRS && used + 8 < len; i++) {
        if(addrs[i] == LIVE_TEST_ADDR_NONE) break;
        used += (size_t)snprintf(out + used, len - used, used ? " or 0x%02X" : "0x%02X", addrs[i]);
    }
}

// OK on a row: work out where the part is, then run the test there.
static void tests_launch(FakeChipApp* app, size_t index) {
    const LiveTest* test = NULL;
    const uint8_t* addrs = NULL;
    char file[LIVE_PLUGIN_FILE_LEN] = {0};
    bool from_card = false;

    with_view_model(
        app->tests_view,
        TestsViewModel * m,
        {
            const LivePluginInfo* plugin = tests_row_plugin(m, index);
            if(plugin) {
                from_card = true;
                if(plugin->status == LivePluginOk) {
                    strlcpy(file, plugin->file, sizeof(file));
                    addrs = plugin->addrs;
                }
            } else {
                test = live_test_get(index);
                if(test) addrs = test->addrs;
            }
        },
        false);

    const char* problem = NULL;
    char note[LIVE_TEST_LINE_LEN] = {0};

    if(from_card && !file[0]) {
        problem = "That one will not load";
    } else if(!addrs) {
        problem = "No addresses to try";
    } else {
        // Copied out before the probe: the model lock is not held during a bus
        // transfer, and `addrs` for a plugin points into the model.
        uint8_t candidates[LIVE_TEST_MAX_ADDRS];
        memcpy(candidates, addrs, sizeof(candidates));

        uint8_t addr = tests_probe(candidates);
        if(!addr) {
            // Sized so "Nothing at " plus the widest list this can produce is
            // provably inside a 26-character line.
            char where[15];
            tests_describe_addrs(where, sizeof(where), candidates);
            snprintf(note, sizeof(note), "Nothing at %s", where);
            problem = note;
        } else if(from_card) {
            // Loaded here and closed in live_exit_callback, once the worker
            // that is executing its code has been joined.
            LivePluginStatus status = LivePluginOk;
            LivePluginHandle* handle = live_plugin_open(file, &status);
            if(!handle) {
                problem = live_plugin_status_text(status);
            } else {
                app->live_plugin = handle;
                app_start_live_test(app, live_plugin_test(handle), addr, FakeChipViewTests);
                return;
            }
        } else {
            app_start_live_test(app, test, addr, FakeChipViewTests);
            return;
        }
    }

    with_view_model(
        app->tests_view,
        TestsViewModel * m,
        { strlcpy(m->message, problem, sizeof(m->message)); },
        true);
}

static bool tests_input_callback(InputEvent* event, void* context) {
    FakeChipApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = false;
    bool help = false;
    size_t launch = SIZE_MAX;

    with_view_model(
        app->tests_view,
        TestsViewModel * m,
        {
            size_t total = tests_total(m);
            // This list wraps, unlike the others. The instructions live on the
            // last row, and without wrapping a user hunting for them has to
            // scroll past every test in the app to find out how to add one.
            if(event->key == InputKeyUp) {
                m->selected = (uint16_t)(m->selected ? (size_t)m->selected - 1 : total - 1);
                m->message[0] = '\0';
                consumed = true;
            } else if(event->key == InputKeyDown) {
                m->selected = (uint16_t)(((size_t)m->selected + 1) % total);
                m->message[0] = '\0';
                consumed = true;
            } else if(event->key == InputKeyOk) {
                m->message[0] = '\0';
                if(tests_row_is_help(m, m->selected)) {
                    help = true;
                } else {
                    launch = m->selected;
                }
                consumed = true;
            }
        },
        consumed);

    // Both of these switch views, so they happen outside the model lock.
    if(help) app_switch_view(app, FakeChipViewTestHelp);
    if(launch != SIZE_MAX) tests_launch(app, launch);
    return consumed;
}

static void tests_enter_callback(void* context) {
    FakeChipApp* app = context;
    // Reading each plugin's name means mapping its ELF, so this costs a moment
    // per file. Doing it on entry rather than on every draw is the difference
    // between a screen that opens slowly once and one that never settles.
    LivePluginList* list = malloc(sizeof(LivePluginList));
    live_plugin_list(list);

    with_view_model(
        app->tests_view,
        TestsViewModel * m,
        {
            free(m->plugins);
            m->plugins = list;
            m->message[0] = '\0';
            if(m->selected >= tests_total(m)) m->selected = 0;
        },
        true);
}

static void tests_exit_callback(void* context) {
    FakeChipApp* app = context;
    with_view_model(
        app->tests_view,
        TestsViewModel * m,
        {
            free(m->plugins);
            m->plugins = NULL;
        },
        false);
}

static void test_help_draw_callback(Canvas* canvas, void* model) {
    UNUSED(model);
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Add your own test");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 22, "Build a .fal with ufbt and");
    canvas_draw_str(canvas, 2, 31, "copy it to the SD card at");
    canvas_draw_str(canvas, 2, 42, "apps_data/");
    canvas_draw_str(canvas, 2, 51, "  fake_chip_detector/tests");

    canvas_draw_line(canvas, 0, 54, 128, 54);
    canvas_draw_str(canvas, 2, 62, "Template + guide: see repo");
}

/* ---------------- UART self-test ---------------- */

static void selftest_draw_callback(Canvas* canvas, void* model) {
    SelftestViewModel* m = model;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "UART self-test");

    // The setup stays on screen beside the result rather than being replaced by
    // it. It is what makes a failure actionable, and it is also the whole answer
    // to the blocked state, so clearing it the moment a result arrives would
    // take away the fix at the exact moment it is needed.
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 22, "Jumper pin 15 to pin 16,");
    canvas_draw_str(canvas, 2, 31, "nothing else on the bus.");

    canvas_draw_line(canvas, 0, 36, 128, 36);

    const char* headline;
    const char* detail = m->detail;
    if(m->running) {
        headline = "Running...";
        detail = "";
    } else if(m->state == I2CSelftestPassed) {
        headline = "Passed";
    } else if(m->state == I2CSelftestFailed) {
        headline = "Failed";
    } else if(m->state == I2CSelftestBlocked) {
        // Deliberately not "Failed". Nothing was measured, so nothing failed.
        headline = "Not run";
    } else {
        headline = "Not run yet";
        detail = "Press OK to start";
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 48, headline);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 59, detail);
}

static bool selftest_input_callback(InputEvent* event, void* context) {
    FakeChipApp* app = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        // Everything else the worker does owns these same two pins. Nothing
        // reaches this screen while one of them is running, but queueing behind
        // one would start the test at some unpredictable later moment, and a
        // self-test that runs when it was not asked for is worse than one that
        // declines.
        if(i2c_worker_is_busy(app->worker)) return true;
        with_view_model(app->selftest_view, SelftestViewModel * m, { m->running = true; }, true);
        i2c_worker_selftest_start(app->worker);
        return true;
    }
    return false;
}

static void selftest_enter_callback(void* context) {
    FakeChipApp* app = context;
    char detail[I2C_SELFTEST_DETAIL_SIZE];
    I2CSelftestState state = i2c_worker_get_selftest(app->worker, detail, sizeof(detail));
    // running is left alone on purpose: a test still in flight because the user
    // walked out and back in is still in flight, and the done event clears it
    // wherever they happen to be standing.
    with_view_model(
        app->selftest_view,
        SelftestViewModel * m,
        {
            m->state = (uint8_t)state;
            snprintf(m->detail, sizeof(m->detail), "%s", detail);
        },
        true);
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
    report_build(app->report_text, snapshot, count, disputed, &dt, NULL);
    free(snapshot);

    app_present_report(app, nav_to_scan);
}

/* ---------------- Saved reports ---------------- */

#define SAVED_LIST_ROWS 4

// Reports are evidence, and evidence you cannot open is useless. The app that
// wrote them can read them back.
static void saved_reload(FakeChipApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    // No trailing slash: APP_DATA_PATH("") yields "/data/", and storage_dir_open
    // will not open that.
    FuriString* dir = furi_string_alloc_set(STORAGE_APP_DATA_PATH_PREFIX);
    storage_common_resolve_path_and_ensure_app_directory(storage, dir);

    File* d = storage_file_alloc(storage);
    char(*names)[SAVED_NAME_LEN] = malloc(SAVED_MAX * SAVED_NAME_LEN);
    uint8_t count = 0;
    uint16_t total = 0;

    if(storage_dir_open(d, furi_string_get_cstr(dir))) {
        FileInfo info;
        char name[SAVED_NAME_LEN];
        while(storage_dir_read(d, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            if(strncmp(name, REPORT_FILE_PREFIX, strlen(REPORT_FILE_PREFIX)) != 0) continue;
            total++;

            // Newest first, and the newest are the ones kept. Stopping at the
            // first SAVED_MAX the directory happened to hand over threw away
            // whichever were newest — so the user saved a report, opened this
            // screen and could not find the one they had just made. The name
            // carries the timestamp in a form that sorts, so "newer" is only
            // "greater", and an insertion into a list this short costs nothing
            // next to the directory read it sits inside.
            if(count == SAVED_MAX && strcmp(name, names[SAVED_MAX - 1]) <= 0) continue;

            uint8_t pos = count < SAVED_MAX ? count : (uint8_t)(SAVED_MAX - 1);
            while(pos > 0 && strcmp(names[pos - 1], name) < 0) {
                memcpy(names[pos], names[pos - 1], SAVED_NAME_LEN);
                pos--;
            }
            snprintf(names[pos], SAVED_NAME_LEN, "%s", name);
            if(count < SAVED_MAX) count++;
        }
        storage_dir_close(d);
    }
    storage_file_free(d);
    furi_string_free(dir);
    furi_record_close(RECORD_STORAGE);

    // Swap the finished list in under a short lock rather than reading the
    // directory with the model held.
    char(*old)[SAVED_NAME_LEN] = NULL;
    with_view_model(
        app->saved_view,
        SavedViewModel * m,
        {
            old = m->names;
            m->names = names;
            m->count = count;
            m->skipped = (uint16_t)(total - count);
            if(m->selected >= count) m->selected = count ? (uint8_t)(count - 1) : 0;
        },
        true);
    free(old);
}

// The list is rebuilt on every entry, not just the first: the screen is also
// reached by backing out of a report, and a scan may have saved one since.
static void saved_enter(void* context) {
    saved_reload(context);
}

static void saved_exit(void* context) {
    FakeChipApp* app = context;
    with_view_model(
        app->saved_view,
        SavedViewModel * m,
        {
            free(m->names);
            m->names = NULL;
            m->count = 0;
        },
        false);
}

static void saved_open(FakeChipApp* app) {
    char name[SAVED_NAME_LEN] = {0};
    with_view_model(
        app->saved_view,
        SavedViewModel * m,
        {
            if(m->names && m->count) snprintf(name, sizeof(name), "%s", m->names[m->selected]);
        },
        false);
    if(!name[0]) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* path = furi_string_alloc_printf(APP_DATA_PATH("%s"), name);
    storage_common_resolve_path_and_ensure_app_directory(storage, path);

    furi_string_reset(app->report_text);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        // Capped: a real report is a couple of kilobytes, and a corrupt or
        // hand-edited file must not be able to exhaust the heap.
        char chunk[257];
        size_t n;
        size_t total = 0;
        while(total < REPORT_READ_MAX &&
              (n = storage_file_read(f, chunk, sizeof(chunk) - 1)) > 0) {
            chunk[n] = 0;
            furi_string_cat_str(app->report_text, chunk);
            total += n;
        }
        if(total >= REPORT_READ_MAX) {
            furi_string_cat_str(app->report_text, "\n[report truncated]\n");
        }
    } else {
        furi_string_set_str(app->report_text, "Could not open this report.");
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_string_free(path);
    furi_record_close(RECORD_STORAGE);

    app_present_report(app, nav_to_saved);
}

static void saved_draw_callback(Canvas* canvas, void* model) {
    SavedViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Saved reports");

    if(m->count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignBottom, "Nothing saved yet.");
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignBottom, "Reports show up here");
        canvas_draw_str_aligned(canvas, 64, 51, AlignCenter, AlignBottom, "once you save one.");
        return;
    }

    char buf[20];
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%u/%u", m->selected + 1, m->count);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, buf);

    uint8_t first = 0;
    if(m->selected >= SAVED_LIST_ROWS) first = (uint8_t)(m->selected - SAVED_LIST_ROWS + 1);
    for(uint8_t row = 0; row < SAVED_LIST_ROWS; row++) {
        uint8_t idx = (uint8_t)(first + row);
        if(idx >= m->count) break;
        uint8_t y = (uint8_t)(22 + row * 10);
        report_filename_label(m->names[idx], buf, sizeof(buf));
        if(idx == m->selected) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 4, y, buf);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 4, y, buf);
        }
    }

    // What was left out is said, not hidden. A list that quietly stops at 32 of
    // 40 reads as "that is all of them".
    if(m->skipped) {
        char bar[32];
        snprintf(bar, sizeof(bar), "OK: read it - %u older", m->skipped);
        draw_action_bar(canvas, bar, false);
    } else {
        draw_action_bar(canvas, "OK: read it", false);
    }
}

static bool saved_input_callback(InputEvent* event, void* context) {
    FakeChipApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    bool consumed = false, open = false;
    with_view_model(
        app->saved_view,
        SavedViewModel * m,
        {
            if(event->key == InputKeyUp && m->selected > 0) {
                m->selected--;
                consumed = true;
            } else if(event->key == InputKeyDown && (uint8_t)(m->selected + 1) < m->count) {
                m->selected++;
                consumed = true;
            } else if(event->key == InputKeyOk && m->count) {
                open = true;
                consumed = true;
            }
        },
        consumed);
    if(open) saved_open(app);
    return consumed;
}

/* ---------------- 1-Wire ---------------- */

// The 1-Wire bus lives on its own pin (17) and has its own failure modes, so
// it gets its own screen rather than being folded into the I2C scan.

static int32_t ow_thread_worker(void* context) {
    FakeChipApp* app = context;

    // Scanned into a local first: the search holds the bus for up to a second
    // and the view model must not be locked for anything like that long.
    OneWireScanResult res;
    onewire_worker_scan(&res, &app->ow_abort);

    with_view_model(
        app->onewire_view,
        OneWireViewModel * m,
        {
            m->res = res;
            m->busy = false;
        },
        true);

    if(!app->ow_abort) {
        i2c_notify_play(
            app->notifications,
            res.count                      ? I2CNotifyNeutral :
            res.state == OneWireBusShorted ? I2CNotifyBad :
                                             I2CNotifyAttention);
    }
    return 0;
}

static void onewire_enter(void* context) {
    FakeChipApp* app = context;
    with_view_model(
        app->onewire_view,
        OneWireViewModel * m,
        {
            memset(&m->res, 0, sizeof(m->res));
            m->selected = 0;
            m->busy = true;
            m->explain = false;
            m->saved_name[0] = '\0';
            m->save_failed = false;
        },
        true);

    app->ow_abort = false;
    app->ow_thread = furi_thread_alloc_ex("FakeChipOneWire", 2048, ow_thread_worker, app);
    furi_thread_start(app->ow_thread);
}

static void onewire_exit(void* context) {
    FakeChipApp* app = context;
    if(!app->ow_thread) return;
    app->ow_abort = true;
    furi_thread_join(app->ow_thread);
    furi_thread_free(app->ow_thread);
    app->ow_thread = NULL;
}

// Same rule as the scan side: snapshot the result, then build and write the
// report outside the model lock. An SD write can stall for seconds and the GUI
// must not be holding the mutex while it does.
static void onewire_save_log(FakeChipApp* app) {
    OneWireScanResult* snapshot = malloc(sizeof(OneWireScanResult));
    with_view_model(app->onewire_view, OneWireViewModel * m, { *snapshot = m->res; }, false);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    FuriString* text = furi_string_alloc();
    report_build_onewire(text, snapshot, &dt);
    free(snapshot);

    char name[SAVED_NAME_LEN] = {0};
    bool saved = report_save_text(text, &dt, name, sizeof(name));
    furi_string_free(text);

    with_view_model(
        app->onewire_view,
        OneWireViewModel * m,
        {
            snprintf(m->saved_name, sizeof(m->saved_name), "%s", saved ? name : "");
            m->save_failed = !saved;
        },
        true);
}

// 16 hex digits with no separators: it is an identifier to compare, not prose.
static void ow_format_rom(const uint8_t* rom, char out[17]) {
    for(uint8_t i = 0; i < 8; i++) {
        snprintf(out + i * 2, 3, "%02X", rom[i]);
    }
}

static void onewire_draw_callback(Canvas* canvas, void* model) {
    OneWireViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    // Saving a file and not saying where it went is not saving it — and a
    // write that failed has to say so just as plainly, or the user leaves
    // holding evidence that was never written.
    if(m->saved_name[0] || m->save_failed) {
        if(m->save_failed) {
            canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignBottom, "SAVE FAILED");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas, 64, 30, AlignCenter, AlignBottom, "The SD card did not take");
            canvas_draw_str_aligned(
                canvas, 64, 41, AlignCenter, AlignBottom, "the file. Card missing,");
            canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignBottom, "or out of space?");
        } else {
            canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignBottom, "REPORT SAVED");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignBottom, m->saved_name);
            canvas_draw_str_aligned(
                canvas, 64, 40, AlignCenter, AlignBottom, "On the SD card, in");
            canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignBottom, "apps_data/");
            canvas_draw_str_aligned(
                canvas, 64, 58, AlignCenter, AlignBottom, "fake_chip_detector");
        }
        return;
    }

    // Saying "IDs are copyable" and leaving it there would be worse than not
    // saying it: the whole point of the app is that the user understands what
    // the verdict rests on.
    if(m->explain) {
        canvas_draw_str(canvas, 2, 10, "What this proves");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 22, "The 64-bit ID is burned");
        canvas_draw_str(canvas, 2, 31, "in at the factory, but");
        canvas_draw_str(canvas, 2, 40, "any chip can replay it.");
        canvas_draw_str(canvas, 2, 49, "It shows which part this");
        canvas_draw_str(canvas, 2, 58, "is, not who made it.");
        return;
    }

    if(m->busy) {
        canvas_draw_str(canvas, 2, 10, "1-Wire");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignBottom, "Searching the bus...");
        canvas_draw_str_aligned(
            canvas, 64, 44, AlignCenter, AlignBottom, "Measuring takes a second.");
        return;
    }

    if(m->res.count == 0) {
        canvas_draw_str(canvas, 2, 10, "Nothing on 1-Wire");
        canvas_set_font(canvas, FontSecondary);
        if(m->res.state == OneWireBusShorted) {
            canvas_draw_str(canvas, 2, 24, "Pin 17 is held low.");
            canvas_draw_str(canvas, 2, 34, "Shorted to GND, or the");
            canvas_draw_str(canvas, 2, 44, "4.7k pull-up is missing.");
        } else {
            canvas_draw_str(canvas, 2, 24, "No device answered.");
            canvas_draw_str(canvas, 2, 34, "Data to pin 17, plus 3V3,");
            canvas_draw_str(canvas, 2, 44, "GND and a 4.7k pull-up.");
        }
        // Saving matters most here, not least. The person holding a part that
        // said nothing at all is the one being asked to accept or refuse a
        // parcel with no evidence either way, and the report has something to
        // give them: whether the line was held low, which is usually their own
        // wiring, or whether it was idle and simply nobody answered. There is
        // no OK action on this screen, so the bar carries only the save hint.
        draw_action_bar(canvas, "", true);
        return;
    }

    const OneWireDevice* dev = &m->res.found[m->selected];

    canvas_draw_str(canvas, 2, 10, dev->name ? dev->name : "Unknown part");
    if(m->res.count > 1 || m->res.overflow) {
        char pos[12];
        snprintf(
            pos, sizeof(pos), "%u/%u%s", m->selected + 1, m->res.count, m->res.overflow ? "+" : "");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, pos);
    }

    canvas_set_font(canvas, FontSecondary);
    if(dev->kind) {
        canvas_draw_str(canvas, 2, 21, dev->kind);
    } else {
        char fam[24];
        snprintf(fam, sizeof(fam), "Family code 0x%02X", dev->rom[0]);
        canvas_draw_str(canvas, 2, 21, fam);
    }

    char rom[17];
    ow_format_rom(dev->rom, rom);
    canvas_draw_str(canvas, 2, 32, rom);

    if(!dev->crc_ok) {
        canvas_draw_str(canvas, 2, 43, "ID checksum is wrong!");
    } else if(dev->measured && dev->scratch_crc_ok) {
        // Outside the DS18B20's own -55..+125 range the number is not a
        // temperature, and claiming "it works" from it would be a lie.
        int tenths = (int)(dev->temp_c * 10.0f);
        if(tenths < -550 || tenths > 1250) {
            canvas_draw_str(canvas, 2, 43, "Reading out of range.");
        } else {
            // The sign is printed on its own, not carried by tenths/10. C
            // truncates toward zero, so anything from -0.9 to -0.1 C has a
            // whole part of 0 -- and "0" has no minus in it. Formatted the
            // obvious way, a sensor sitting at half a degree below freezing
            // reported half a degree above it.
            int mag = tenths < 0 ? -tenths : tenths;
            char line[26];
            snprintf(
                line,
                sizeof(line),
                "Reads %s%d.%d C - it works",
                tenths < 0 ? "-" : "",
                mag / 10,
                mag % 10);
            canvas_draw_str(canvas, 2, 43, line);
        }
    } else if(dev->measured) {
        canvas_draw_str(canvas, 2, 43, "Answered, data corrupt.");
    } else {
        canvas_draw_str(canvas, 2, 43, "Present, ID checks out.");
    }

    // Shorter than the "OK: what this proves" this screen used to say: the
    // save hint and its key glyph now take the right-hand end of the bar, and
    // the only other call that offers save gets by with a label this length.
    draw_action_bar(canvas, "OK: explain", true);
}

static bool onewire_input_callback(InputEvent* event, void* context) {
    FakeChipApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    bool consumed = false;
    bool do_save = false;
    with_view_model(
        app->onewire_view,
        OneWireViewModel * m,
        {
            if(m->saved_name[0] || m->save_failed) {
                // any key dismisses the save confirmation, Back included, so
                // the first Back returns to the result rather than the menu
                m->saved_name[0] = '\0';
                m->save_failed = false;
                consumed = true;
            } else if(m->explain) {
                // Any key closes the panel, Back included — consuming it here
                // means the first Back returns to the result, not to the menu.
                // Checked before the save key so that promise stays literal.
                m->explain = false;
                consumed = true;
            } else if(event->key == InputKeyRight && !m->busy && event->type == InputTypeShort) {
                // Deliberately not gated on a device having been found: an
                // empty bus is a result too, and it is the one its owner most
                // needs written down. Gated on !busy so a half-finished scan
                // can never be filed as a finished one.
                do_save = true;
                consumed = true;
            } else if(event->key == InputKeyUp && m->selected > 0) {
                m->selected--;
                consumed = true;
            } else if(event->key == InputKeyDown && (uint8_t)(m->selected + 1) < m->res.count) {
                m->selected++;
                consumed = true;
            } else if(event->key == InputKeyOk && m->res.count) {
                m->explain = true;
                consumed = true;
            }
        },
        consumed);

    if(do_save) {
        onewire_save_log(app);
        i2c_notify_play(app->notifications, I2CNotifyNeutral);
    }
    return consumed;
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
        case FakeChipViewSilent:
            with_view_model(app->silent_view, SilentViewModel * m, { m->frame++; }, true);
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
    case MenuIndexOneWire:
        app_switch_view(app, FakeChipViewOneWire);
        break;
    case MenuIndexTests:
        app_switch_view(app, FakeChipViewTests);
        break;
    case MenuIndexSelftest:
        app_switch_view(app, FakeChipViewSelftest);
        break;
    case MenuIndexSettings:
        app_switch_view(app, FakeChipViewSettings);
        break;
    case MenuIndexChips:
        app_switch_view(app, FakeChipViewChips);
        break;
    case MenuIndexSaved:
        app_switch_view(app, FakeChipViewSaved);
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

static uint32_t nav_to_tests(void* context) {
    UNUSED(context);
    return FakeChipViewTests;
}

// A live test has two ways in: the verdict screen after a scan, and the
// browser. Sending Back to a fixed destination dumped anyone who came from
// the browser onto a stale scan result they had already dealt with.
static uint32_t nav_from_live(void* context) {
    FakeChipApp* app = context;
    return app->live_return_to;
}

static uint32_t nav_to_menu(void* context) {
    UNUSED(context);
    return FakeChipViewMenu;
}

// Which screen the animation thread should be ticking. The dispatcher runs the
// outgoing view's exit callback *after* app_switch_view has already published
// the incoming one, so an exit callback that assigns unconditionally undoes the
// switch that is happening right now — which is how the live test ended up
// launching from the verdict screen with its frame counter stuck at zero. An
// enter callback runs last and can assign freely; an exit callback may only
// stand down if nobody else has claimed the slot.
static void scan_enter_callback(void* context) {
    FakeChipApp* app = context;
    app->current_view = FakeChipViewScan;
}

static void scan_exit_callback(void* context) {
    FakeChipApp* app = context;
    if(app->current_view == FakeChipViewScan) app->current_view = FakeChipViewMenu;
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
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_custom_event_callback);
    app->current_view = FakeChipViewMenu;

    if(app->settings.backlight) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }

    app->worker = i2c_worker_alloc();
    i2c_worker_set_callback(app->worker, worker_event_callback, app);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Fake Chip Detector");
    submenu_add_item(app->submenu, "How to wire", MenuIndexWiring, menu_callback, app);
    submenu_add_item(app->submenu, "Scan I2C bus", MenuIndexScan, menu_callback, app);
    submenu_add_item(app->submenu, "Scan 1-Wire", MenuIndexOneWire, menu_callback, app);
    submenu_add_item(app->submenu, "Live tests", MenuIndexTests, menu_callback, app);
    submenu_add_item(app->submenu, "UART self-test", MenuIndexSelftest, menu_callback, app);
    submenu_add_item(app->submenu, "Settings", MenuIndexSettings, menu_callback, app);
    submenu_add_item(app->submenu, "Known chips", MenuIndexChips, menu_callback, app);
    submenu_add_item(app->submenu, "Saved reports", MenuIndexSaved, menu_callback, app);
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
    view_set_enter_callback(app->scan_view, scan_enter_callback);
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
    // Back returns wherever the test was started from — the verdict that
    // offered it, or the browser it was picked out of.
    view_set_previous_callback(app->live_view, nav_from_live);
    view_dispatcher_add_view(app->view_dispatcher, FakeChipViewLive, app->live_view);

    app->settings_list = variable_item_list_alloc();
    settings_build(app);
    view_set_previous_callback(variable_item_list_get_view(app->settings_list), nav_to_menu);
    view_dispatcher_add_view(
        app->view_dispatcher,
        FakeChipViewSettings,
        variable_item_list_get_view(app->settings_list));

    app->tests_view = view_alloc();
    view_set_context(app->tests_view, app);
    view_allocate_model(app->tests_view, ViewModelTypeLocking, sizeof(TestsViewModel));
    view_set_draw_callback(app->tests_view, tests_draw_callback);
    view_set_input_callback(app->tests_view, tests_input_callback);
    view_set_enter_callback(app->tests_view, tests_enter_callback);
    view_set_exit_callback(app->tests_view, tests_exit_callback);
    view_set_previous_callback(app->tests_view, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, FakeChipViewTests, app->tests_view);

    app->test_help_view = view_alloc();
    view_set_context(app->test_help_view, app);
    view_set_draw_callback(app->test_help_view, test_help_draw_callback);
    view_set_previous_callback(app->test_help_view, nav_to_tests);
    view_dispatcher_add_view(app->view_dispatcher, FakeChipViewTestHelp, app->test_help_view);

    app->selftest_view = view_alloc();
    view_set_context(app->selftest_view, app);
    view_allocate_model(app->selftest_view, ViewModelTypeLocking, sizeof(SelftestViewModel));
    view_set_draw_callback(app->selftest_view, selftest_draw_callback);
    view_set_input_callback(app->selftest_view, selftest_input_callback);
    view_set_enter_callback(app->selftest_view, selftest_enter_callback);
    view_set_previous_callback(app->selftest_view, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, FakeChipViewSelftest, app->selftest_view);

    app->chips_view = view_alloc();
    view_set_context(app->chips_view, app);
    view_allocate_model(app->chips_view, ViewModelTypeLocking, sizeof(ChipsViewModel));
    view_set_draw_callback(app->chips_view, chips_draw_callback);
    view_set_input_callback(app->chips_view, chips_input_callback);
    view_set_previous_callback(app->chips_view, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, FakeChipViewChips, app->chips_view);

    app->onewire_view = view_alloc();
    view_set_context(app->onewire_view, app);
    view_allocate_model(app->onewire_view, ViewModelTypeLocking, sizeof(OneWireViewModel));
    view_set_draw_callback(app->onewire_view, onewire_draw_callback);
    view_set_input_callback(app->onewire_view, onewire_input_callback);
    view_set_enter_callback(app->onewire_view, onewire_enter);
    view_set_exit_callback(app->onewire_view, onewire_exit);
    view_set_previous_callback(app->onewire_view, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, FakeChipViewOneWire, app->onewire_view);

    app->silent_view = view_alloc();
    view_set_context(app->silent_view, app);
    view_allocate_model(app->silent_view, ViewModelTypeLocking, sizeof(SilentViewModel));
    view_set_draw_callback(app->silent_view, silent_draw_callback);
    view_set_input_callback(app->silent_view, silent_input_callback);
    view_set_enter_callback(app->silent_view, silent_enter_callback);
    view_set_exit_callback(app->silent_view, silent_exit_callback);
    view_set_previous_callback(app->silent_view, nav_to_scan);
    view_dispatcher_add_view(app->view_dispatcher, FakeChipViewSilent, app->silent_view);

    app->saved_view = view_alloc();
    view_set_context(app->saved_view, app);
    view_allocate_model(app->saved_view, ViewModelTypeLocking, sizeof(SavedViewModel));
    view_set_draw_callback(app->saved_view, saved_draw_callback);
    view_set_input_callback(app->saved_view, saved_input_callback);
    view_set_enter_callback(app->saved_view, saved_enter);
    view_set_exit_callback(app->saved_view, saved_exit);
    view_set_previous_callback(app->saved_view, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, FakeChipViewSaved, app->saved_view);

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
        // Two numbers rather than one total: they are not the same kind of
        // knowledge. An I2C chip is named from an ID register the app reads
        // back; a 1-Wire part is named from a family code that identifies the
        // part without vouching for it.
        static char db_line[32];
        snprintf(
            db_line,
            sizeof(db_line),
            "%u I2C + %u 1-Wire known",
            (unsigned)chip_db_count(),
            (unsigned)onewire_family_count());
        widget_add_string_element(
            app->about_widget, 64, 42, AlignCenter, AlignTop, FontSecondary, db_line);
    }
    widget_add_string_element(
        app->about_widget, 64, 52, AlignCenter, AlignTop, FontSecondary, "pin16 SCL/15 SDA - MIT");
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
    // A live test should already have been torn down by the view's exit
    // callback, because leaving the screen is the only way to reach here and
    // that always navigates. "Should" is doing real work in that sentence
    // though: it holds only while nothing installs an input callback on
    // live_view or stops the dispatcher from elsewhere, and neither is
    // something the next person to touch this file would expect to be
    // load-bearing. What it would cost is a mapped plugin leaked and a thread
    // still executing code inside it. So call the teardown again — it is
    // idempotent, and it keeps the join-then-unmap order in exactly one place.
    live_exit_callback(app);

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
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewSaved);
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewOneWire);
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewSilent);
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewTests);
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewTestHelp);
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewSelftest);
    view_dispatcher_remove_view(app->view_dispatcher, FakeChipViewAbout);
    submenu_free(app->submenu);
    view_free(app->wiring_view);
    view_free(app->scan_view);
    view_free(app->detail_view);
    view_free(app->live_view);
    view_free(app->tests_view);
    view_free(app->test_help_view);
    view_free(app->selftest_view);
    view_free(app->chips_view);
    view_free(app->saved_view);
    view_free(app->onewire_view);
    view_free(app->silent_view);
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
