#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#include "usb_audio.h"
#include "i2s_out.h"

#define TAG "UsbI2s"

#define VBUS_PRESENT_V 4.0f
#define TICK_MS        250
#define WIRING_PAGES   5

#define SETTINGS_PATH    APP_DATA_PATH("settings.bin")
#define SETTINGS_MAGIC   0x12
#define SETTINGS_VERSION 1

typedef struct {
    uint8_t clock_source;
} UsbI2sSettings;

typedef enum {
    ViewIdMenu,
    ViewIdStatus,
    ViewIdWiring,
    ViewIdSettings,
    ViewIdAbout,
} ViewId;

typedef enum {
    MenuIdStatus,
    MenuIdWiring,
    MenuIdSettings,
    MenuIdAbout,
} MenuId;

typedef struct {
    UsbAudioStats st;
    bool vbus;
    bool usb_ok;
    uint16_t peak_l; // decaying meter
    uint16_t peak_r;
} StatusModel;

typedef struct {
    uint8_t page;
} WiringModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* menu;
    View* status;
    View* wiring;
    VariableItemList* settings;
    Widget* about;
    bool usb_ok;
    UsbI2sSettings cfg;
} UsbI2sApp;

/* ---------- Status ---------- */

static void draw_meter(Canvas* canvas, int32_t x, const char* label, uint16_t peak) {
    // Half-width meter on the bottom row: label, then 55 px bar
    const int32_t y = 57, w = 55;
    canvas_draw_str(canvas, x, 63, label);
    canvas_draw_frame(canvas, x + 7, y, w, 6);
    int32_t fill = (int32_t)peak * (w - 2) / 32768;
    if(fill > 0) canvas_draw_box(canvas, x + 8, y + 1, fill, 4);
}

static void status_draw_callback(Canvas* canvas, void* _model) {
    StatusModel* m = _model;
    const UsbAudioStats* st = &m->st;
    char buf[40];

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 9, "USB-I2S");

    const char* state;
    if(!m->usb_ok) {
        state = "USB BUSY";
    } else if(!m->vbus) {
        state = "NO CABLE";
    } else if(!st->configured) {
        state = "WAIT HOST";
    } else if(st->suspended) {
        state = "SUSPEND";
    } else if(st->streaming && st->receiving) {
        state = "PLAYING";
    } else {
        state = "IDLE";
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 127, 1, AlignRight, AlignTop, state);
    canvas_draw_line(canvas, 0, 11, 127, 11);

    if(!m->usb_ok) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "USB is locked");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, "Close other USB apps");
        return;
    }

    if(!m->vbus) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "No USB cable");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, "Connect Flipper to a");
        canvas_draw_str_aligned(canvas, 64, 51, AlignCenter, AlignCenter, "computer or phone");
        return;
    }

    if(!st->configured || st->suspended) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas,
            64,
            28,
            AlignCenter,
            AlignCenter,
            st->suspended ? "Host is asleep" : "Enumerating...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, "Cable detected,");
        canvas_draw_str_aligned(canvas, 64, 51, AlignCenter, AlignCenter, "waiting for host");
        return;
    }

    // Requested rate, large
    canvas_set_font(canvas, FontPrimary);
    snprintf(
        buf, sizeof(buf), "%lu.%lu kHz", st->sample_rate / 1000, (st->sample_rate % 1000) / 100);
    canvas_draw_str(canvas, 0, 22, buf);
    canvas_set_font(canvas, FontSecondary);
    // Bit depth arrives only with SET_INTERFACE when the host opens the stream
    if(st->streaming) {
        snprintf(buf, sizeof(buf), "%ubit 2ch", st->bits);
    } else {
        snprintf(buf, sizeof(buf), "--bit 2ch");
    }
    canvas_draw_str_aligned(canvas, 127, 22, AlignRight, AlignBottom, buf);

    if(!st->streaming) {
        canvas_draw_str(canvas, 0, 33, "Host connected, idle");
        canvas_draw_str(canvas, 0, 42, "Select Flipper USB-I2S");
        snprintf(buf, sizeof(buf), "Clock: %s", i2s_out_source_name(i2s_out_get_source()));
        canvas_draw_str(canvas, 0, 51, buf);
        snprintf(buf, sizeof(buf), "Vol %d dB%s", st->volume_db256 / 256, st->mute ? " MUTE" : "");
        canvas_draw_str(canvas, 0, 61, buf);
        return;
    }

    const uint32_t clk = st->mclk ? st->mclk : st->bclk;
    snprintf(
        buf,
        sizeof(buf),
        "I2S %luHz %s %lu.%02luM",
        st->actual_rate_mhz / 1000,
        st->mclk ? "MCK" : "BCK",
        clk / 1000000,
        (clk % 1000000) / 10000);
    canvas_draw_str(canvas, 0, 31, buf);

    snprintf(
        buf,
        sizeof(buf),
        "Meas %luHz FB %lu.%03lu",
        st->measured_rate,
        st->feedback >> 14,
        ((st->feedback & 0x3FFF) * 1000) >> 14);
    canvas_draw_str(canvas, 0, 40, buf);

    if(st->mute) {
        snprintf(buf, sizeof(buf), "Buf %lu%% Xrun %lu MUTE", st->fill_pct, st->xruns);
    } else {
        snprintf(
            buf,
            sizeof(buf),
            "Buf %lu%% Xrun %lu %ddB",
            st->fill_pct,
            st->xruns,
            st->volume_db256 / 256);
    }
    canvas_draw_str(canvas, 0, 49, buf);

    draw_meter(canvas, 0, "L", m->peak_l);
    draw_meter(canvas, 66, "R", m->peak_r);
}

static void status_update(UsbI2sApp* app) {
    UsbAudioStats st;
    usb_audio_get_stats(&st);
    bool vbus = furi_hal_power_get_usb_voltage() > VBUS_PRESENT_V;
    with_view_model(
        app->status,
        StatusModel * m,
        {
            m->st = st;
            m->vbus = vbus || st.configured;
            m->usb_ok = app->usb_ok;
            // Fast attack, slow release meter
            m->peak_l = MAX(st.peak_l, m->peak_l / 2);
            m->peak_r = MAX(st.peak_r, m->peak_r / 2);
        },
        true);
}

/* ---------- Wiring ---------- */

static const bool header_used[18] = {
    false,
    false,
    false,
    true,
    true,
    false,
    true,
    true,
    true,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false};

static void wiring_draw_header(Canvas* canvas) {
    // 18 pins, 7 px pitch, pin 1 on the left as seen from the Flipper's top
    for(int32_t i = 0; i < 18; i++) {
        int32_t x = 1 + i * 7;
        if(header_used[i]) {
            canvas_draw_box(canvas, x, 0, 6, 6);
        } else {
            canvas_draw_frame(canvas, x, 0, 6, 6);
        }
    }
    canvas_set_font(canvas, FontKeyboard);
    for(int32_t i = 0; i < 18; i++) {
        if(!header_used[i]) continue;
        char num[3];
        snprintf(num, sizeof(num), "%ld", i + 1);
        canvas_draw_str_aligned(canvas, 4 + i * 7, 7, AlignCenter, AlignTop, num);
    }
}

static void wiring_draw_callback(Canvas* canvas, void* _model) {
    WiringModel* m = _model;
    canvas_clear(canvas);

    if(m->page == 0) {
        wiring_draw_header(canvas);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 25, "Pin 5  PB3 -> BCK  (BCLK)");
        canvas_draw_str(canvas, 0, 34, "Pin 4  PA4 -> LRCK (WS)");
        canvas_draw_str(canvas, 0, 43, "Pin 7  PC3 -> DIN  (DATA)");
        canvas_draw_str(canvas, 0, 52, "Pin 9  3V3 -> VIN/VCC");
        canvas_draw_str(canvas, 0, 61, "Pin 8  GND -> GND");
    } else if(m->page == 1) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 9, "DAC setup");
        canvas_set_font(canvas, FontSecondary);
        // ~5 px per glyph: keep lines under 24 chars
        canvas_draw_str(canvas, 0, 20, "PCM5102A:");
        canvas_draw_str(canvas, 0, 29, " SCK FMT FLT DEMP->GND");
        canvas_draw_str(canvas, 0, 38, " XSMT->3V3 (unmute)");
        canvas_draw_str(canvas, 0, 49, "MAX98357A: SD->VIN");
        canvas_draw_str(canvas, 0, 60, "No MCLK. BCK 32/64 x Fs");
    } else if(m->page == 2) {
        // TDA1541A in I2S (time MUX TWC) mode latches exactly 16 bits before
        // each WS edge, so it needs 16-bit mode (32 x Fs)
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 9, "TDA1541A");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 20, "Pin5 BCK  -> 2 BCK");
        canvas_draw_str(canvas, 0, 29, "Pin4 LRCK -> 1 WS");
        canvas_draw_str(canvas, 0, 38, "Pin7 DIN -> 3  GND -> 14");
        canvas_draw_str(canvas, 0, 47, "27 OB/TWC -> 28 (+5V)");
        canvas_draw_str(canvas, 0, 56, "16-bit only. Own PSU:");
        canvas_draw_str(canvas, 0, 64, "+5V, -5V, -15V");
    } else {
        // CS4398 stand-alone mode, I2S (M1=0 M0=1), TSSOP-28 pin numbers
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 9, m->page == 3 ? "CS4398 signals" : "CS4398 straps");
        canvas_set_font(canvas, FontSecondary);
        if(m->page == 3) {
            canvas_draw_str(canvas, 0, 19, "Pin5  BCK  -> 4 SCLK");
            canvas_draw_str(canvas, 0, 28, "Pin4  LRCK -> 5 LRCK");
            canvas_draw_str(canvas, 0, 37, "Pin7  DIN  -> 3 SDIN");
            canvas_draw_str(canvas, 0, 46, "Pin17 MCLK -> 6 MCLK");
            canvas_draw_str(canvas, 0, 55, "Pin15 -> 9 M3 (speed)");
            canvas_draw_str(canvas, 0, 64, "Pin16 -> 13 RST");
        } else {
            canvas_draw_str(canvas, 0, 19, "3V3 -> 12 M0, 14 VLC,");
            canvas_draw_str(canvas, 0, 28, "       27 VLS");
            canvas_draw_str(canvas, 0, 37, "GND -> 10 M2, 11 M1");
            canvas_draw_str(canvas, 0, 46, "Settings: MCLK out");
            canvas_draw_str(canvas, 0, 55, "Ext osc: osc -> 6 MCLK");
            canvas_draw_str(canvas, 0, 64, " + Pin6, not Pin17");
        }
    }

    canvas_set_font(canvas, FontSecondary);
    char page[6];
    snprintf(page, sizeof(page), "%u/%u", m->page + 1, WIRING_PAGES);
    canvas_draw_str_aligned(canvas, 127, m->page == 0 ? 16 : 0, AlignRight, AlignTop, page);
}

static bool wiring_input_callback(InputEvent* event, void* context) {
    UsbI2sApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    int8_t delta = 0;
    if(event->key == InputKeyRight || event->key == InputKeyDown) delta = 1;
    if(event->key == InputKeyLeft || event->key == InputKeyUp) delta = -1;
    if(!delta) return false;
    with_view_model(
        app->wiring,
        WiringModel * m,
        { m->page = (m->page + WIRING_PAGES + delta) % WIRING_PAGES; },
        true);
    return true;
}

/* ---------- Navigation ---------- */

static uint32_t nav_to_menu(void* context) {
    UNUSED(context);
    return ViewIdMenu;
}

static uint32_t nav_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void menu_callback(void* context, uint32_t index) {
    UsbI2sApp* app = context;
    switch(index) {
    case MenuIdStatus:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdStatus);
        break;
    case MenuIdWiring:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdWiring);
        break;
    case MenuIdSettings:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdSettings);
        break;
    case MenuIdAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdAbout);
        break;
    }
}

static void tick_callback(void* context) {
    status_update(context);
}

/* ---------- Settings ---------- */

static void settings_load(UsbI2sSettings* cfg) {
    if(!saved_struct_load(SETTINGS_PATH, cfg, sizeof(*cfg), SETTINGS_MAGIC, SETTINGS_VERSION) ||
       cfg->clock_source >= I2sClockCount) {
        cfg->clock_source = I2sClockInternal;
    }
}

static void settings_save(const UsbI2sSettings* cfg) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    furi_record_close(RECORD_STORAGE);
    saved_struct_save(SETTINGS_PATH, cfg, sizeof(*cfg), SETTINGS_MAGIC, SETTINGS_VERSION);
}

/* The rate list in the USB descriptor depends on the clock source, so the
 * host must re-enumerate the device. A different interface context makes
 * furi_hal_usb_set_config() run a full deinit / init. */
static void usb_reconnect(I2sClockSource source) {
    furi_hal_usb_disable();
    furi_hal_usb_set_config(NULL, NULL); // stops the stream
    i2s_out_set_source(source);
    usb_audio_build_descriptor();
    furi_hal_usb_set_config(&usb_audio_interface, (void*)(uintptr_t)(source + 1U));
}

static void settings_clock_changed(VariableItem* item) {
    UsbI2sApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, i2s_out_source_name(index));
    app->cfg.clock_source = index;
    settings_save(&app->cfg);
    if(app->usb_ok) usb_reconnect(index);
}

static const char about_text[] =
    "\e#USB-I2S Audio\n"
    "Turns Flipper into a USB sound card (USB Audio Class 1, driverless on "
    "Windows, macOS, Linux, Android) and plays the stream on an external I2S "
    "DAC wired to the GPIO header.\n"
    "\n"
    "\e#Format\n"
    "Stereo PCM, chosen by the host:\n"
    "16-bit: 32 / 44.1 / 48 / 88.2 / 96 kHz\n"
    "24-bit: 32 / 44.1 / 48 kHz\n"
    "24-bit stops at 48 kHz: a 96 kHz packet does not fit the 1 KB USB packet "
    "memory twice.\n"
    "I2S BCK is 32 x Fs for 16-bit, 64 x Fs for 24-bit (32-bit slots).\n"
    "Asynchronous mode: Flipper is the clock master and tells the host its "
    "real rate through a feedback endpoint, so the buffer never drifts.\n"
    "\n"
    "\e#Hardware\n"
    "STM32WB55 SAI1 in I2S mode. Block B makes BCK/LRCK, block A sends data "
    "in sync. Clock comes from PLLSAI1 retuned per rate; USB runs from HSI48 "
    "locked to host SOF by CRS while the app is open.\n"
    "16-bit 32/48 kHz and 24-bit 32 kHz are exact, other modes are within 0.05%.\n"
    "\n"
    "\e#Clock source (Settings)\n"
    "Internal: no MCLK, for DACs with their own clock (PCM5102A, MAX98357A, "
    "TDA1541A).\n"
    "MCLK out: MCLK = 256 x Fs on pin 17, locked to BCK. Rates off by "
    "0.04-0.4%.\n"
    "Ext 24.576M / 22.579M: oscillator into DAC MCLK and pin 6. Exact rates: "
    "48/96 kHz or 44.1/88.2 kHz.\n"
    "Pin 15 = speed (high above 50 kHz), pin 16 = DAC reset, in MCLK modes.\n"
    "\n"
    "\e#Status screen\n"
    "I2S: rate produced by the clocks\n"
    "Meas: rate measured against USB\n"
    "FB: feedback sent to host, samples per ms\n"
    "Buf: buffer fill, Xrun: resyncs\n"
    "\n"
    "Host volume and mute are applied digitally.\n"
    "USB serial/qFlipper is unavailable while the app runs.\n";

/* ---------- App ---------- */

static UsbI2sApp* usb_i2s_app_alloc(void) {
    UsbI2sApp* app = malloc(sizeof(UsbI2sApp));
    settings_load(&app->cfg);
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, tick_callback, TICK_MS);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->menu = submenu_alloc();
    submenu_set_header(app->menu, "USB-I2S Audio");
    submenu_add_item(app->menu, "Status", MenuIdStatus, menu_callback, app);
    submenu_add_item(app->menu, "Wiring", MenuIdWiring, menu_callback, app);
    submenu_add_item(app->menu, "Settings", MenuIdSettings, menu_callback, app);
    submenu_add_item(app->menu, "About", MenuIdAbout, menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->menu), nav_exit);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdMenu, submenu_get_view(app->menu));

    app->status = view_alloc();
    view_allocate_model(app->status, ViewModelTypeLocking, sizeof(StatusModel));
    view_set_draw_callback(app->status, status_draw_callback);
    view_set_previous_callback(app->status, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdStatus, app->status);

    app->wiring = view_alloc();
    view_allocate_model(app->wiring, ViewModelTypeLocking, sizeof(WiringModel));
    view_set_context(app->wiring, app);
    view_set_draw_callback(app->wiring, wiring_draw_callback);
    view_set_input_callback(app->wiring, wiring_input_callback);
    view_set_previous_callback(app->wiring, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdWiring, app->wiring);

    app->settings = variable_item_list_alloc();
    VariableItem* item =
        variable_item_list_add(app->settings, "Clock", I2sClockCount, settings_clock_changed, app);
    variable_item_set_current_value_index(item, app->cfg.clock_source);
    variable_item_set_current_value_text(item, i2s_out_source_name(app->cfg.clock_source));
    view_set_previous_callback(variable_item_list_get_view(app->settings), nav_to_menu);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdSettings, variable_item_list_get_view(app->settings));

    app->about = widget_alloc();
    widget_add_text_scroll_element(app->about, 0, 0, 128, 64, about_text);
    view_set_previous_callback(widget_get_view(app->about), nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdAbout, widget_get_view(app->about));

    return app;
}

static void usb_i2s_app_free(UsbI2sApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdStatus);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdWiring);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdSettings);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdAbout);
    submenu_free(app->menu);
    view_free(app->status);
    view_free(app->wiring);
    variable_item_list_free(app->settings);
    widget_free(app->about);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t usb_i2s_app(void* p) {
    UNUSED(p);
    UsbI2sApp* app = usb_i2s_app_alloc();

    // Stop/sleep modes would switch PLLs off under the running SAI
    furi_hal_power_insomnia_enter();

    FuriHalUsbInterface* usb_prev = furi_hal_usb_get_config();
    app->usb_ok = !furi_hal_usb_is_locked();
    if(app->usb_ok) {
        // Clocks may only be retargeted while the bus is disconnected
        furi_hal_usb_disable();
        i2s_out_init(app->cfg.clock_source);
        usb_audio_build_descriptor();
        app->usb_ok = furi_hal_usb_set_config(
            &usb_audio_interface, (void*)(uintptr_t)(app->cfg.clock_source + 1U));
        if(!app->usb_ok) {
            i2s_out_deinit();
            furi_hal_usb_enable();
        }
    }
    if(!app->usb_ok) FURI_LOG_E(TAG, "USB mode is locked by another app");

    status_update(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdStatus);
    view_dispatcher_run(app->view_dispatcher);

    if(app->usb_ok) {
        furi_hal_usb_disable();
        i2s_out_deinit();
        furi_hal_usb_set_config(usb_prev, NULL);
        furi_hal_usb_enable();
    }

    furi_hal_power_insomnia_exit();
    usb_i2s_app_free(app);
    return 0;
}
