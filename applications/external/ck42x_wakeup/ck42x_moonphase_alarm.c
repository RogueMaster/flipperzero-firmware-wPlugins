#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <furi_hal_infrared.h>
#include <furi_hal_subghz.h>
#include <infrared/worker/infrared_transmit.h>
#include <infrared/worker/infrared_worker.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <storage/storage.h>
#include <string.h>

#define LOOP_MS 100
#define ANIM_TICKS_PER_SECOND 10U
#define IR_MAX_TIMINGS 160U
#define ALARM_COUNT 8U
#define SUBGHZ_FREQ_HZ 433920000UL
#define COUNTER_CYCLE_SECONDS 4U
#define COUNTER_SAVE_DIR STORAGE_APP_DATA_PATH_PREFIX "/ck42x_wakeup"
#define COUNTER_SAVE_FILE COUNTER_SAVE_DIR "/counter.bin"
#define ALARM_SAVE_FILE COUNTER_SAVE_DIR "/alarms.bin"
#define PREFS_SAVE_FILE COUNTER_SAVE_DIR "/prefs.bin"
#define COUNTER_SAVE_MAGIC 0x43534B31UL
#define COUNTER_SAVE_VERSION 1U
#define ALARM_SAVE_MAGIC 0x414C4B31UL
#define ALARM_SAVE_VERSION 1U
#define PREFS_SAVE_MAGIC 0x50524631UL
#define PREFS_SAVE_VERSION 1U

#define MENU_VISIBLE_ROWS 5U
#define MENU_FOOTER_ROWS 4U

typedef enum {
    ScreenMain,
    ScreenMenu,
    ScreenAlarmList,
    ScreenAlarmDetail,
    ScreenTimeSelect,
    ScreenIrMenu,
    ScreenIrCapture,
    ScreenSubgMenu,
    ScreenBadkbMenu,
    ScreenAudioMenu,
    ScreenCounterMenu,
    ScreenAbout,
    ScreenSettings,
    ScreenRing,
} Screen;

typedef enum {
    JumperSheep,
    JumperAliens,
    JumperBees,
    JumperCount,
} JumperType;

typedef enum {
    ToneClassic,
    ToneUrgent,
    ToneRickroll,
    ToneNokia,
    ToneCount,
} AlarmTone;

typedef enum {
    BadkbOff,
    BadkbCk42x,
    BadkbRickroll,
    BadkbWakeSearch,
    BadkbBeeMusic,
    BadkbWakePage,
    BadkbLofiLive,
    BadkbNyanSearch,
    BadkbMorningAlarm,
    BadkbTenHourTimer,
    BadkbRainSounds,
    BadkbPomodoro,
    BadkbCount,
} BadkbPayload;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t counter_count;
} CounterSave;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t configured[ALARM_COUNT];
    uint8_t enabled[ALARM_COUNT];
    uint8_t hour[ALARM_COUNT];
    uint8_t minute[ALARM_COUNT];
    uint8_t ir_enabled[ALARM_COUNT];
    uint8_t subg_enabled[ALARM_COUNT];
    uint8_t haptic_enabled[ALARM_COUNT];
    uint8_t audio_enabled[ALARM_COUNT];
    uint8_t tone[ALARM_COUNT];
    uint8_t badkb_enabled[ALARM_COUNT];
    uint8_t badkb_payload[ALARM_COUNT];
    uint8_t ir_ready[ALARM_COUNT];
    uint8_t ir_start_from_mark[ALARM_COUNT];
    uint16_t ir_count[ALARM_COUNT];
    uint32_t ir_timings[ALARM_COUNT][IR_MAX_TIMINGS];
} AlarmSave;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t backlight_keep_on;
    uint8_t reserved;
} PrefsSave;

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewPort* view_port;
    FuriMessageQueue* queue;
    bool running;
    Screen screen;

    uint32_t seconds;
    uint32_t anim_tick;
    uint8_t hr;
    uint8_t min;
    uint8_t sec;

    uint16_t counter_count;
    uint8_t jumper_frame;
    JumperType jumper_type;
    bool counter_enabled;
    bool backlight_keep_on;

    bool subghz_tx_active;
    bool subghz_last_ok;
    uint16_t subghz_tx_index;
    uint32_t subghz_last_freq;

    bool alarm_configured[ALARM_COUNT];
    bool alarm_enabled[ALARM_COUNT];
    bool alarm_fired_this_minute[ALARM_COUNT];
    uint8_t alarm_hr[ALARM_COUNT];
    uint8_t alarm_min[ALARM_COUNT];
    bool alarm_ir_enabled[ALARM_COUNT];
    bool alarm_subg_enabled[ALARM_COUNT];
    bool alarm_haptic_enabled[ALARM_COUNT];
    bool alarm_audio_enabled[ALARM_COUNT];
    AlarmTone alarm_tone[ALARM_COUNT];
    bool alarm_badkb_enabled[ALARM_COUNT];
    BadkbPayload alarm_badkb_payload[ALARM_COUNT];

    uint8_t selected_alarm;
    uint8_t ringing_alarm;
    uint8_t menu_cursor;
    uint8_t alarm_list_cursor;
    uint8_t detail_cursor;
    uint8_t time_field;
    uint8_t ir_menu_cursor;
    uint8_t subg_menu_cursor;
    uint8_t badkb_menu_cursor;
    uint8_t audio_menu_cursor;
    uint8_t counter_menu_cursor;
    uint8_t settings_cursor;
    uint8_t ring_pulse;

    bool ir_recording;
    uint8_t ir_record_alarm;
    volatile bool ir_capture_done;
    bool ir_ready[ALARM_COUNT];
    volatile uint16_t ir_count[ALARM_COUNT];
    bool ir_start_from_mark[ALARM_COUNT];
    InfraredWorker* ir_worker;
    uint32_t ir_timings[ALARM_COUNT][IR_MAX_TIMINGS];
} Ck42xWakeupApp;

static const uint16_t subghz_ck42x_pulse_us[] = {
    9000, 4500,
    500, 500, 500, 1500, 500, 500, 500, 1500,
    500, 1500, 500, 500, 500, 1500, 500, 500,
    500, 500, 500, 1500, 500, 1500, 500, 500,
    500, 1500, 500, 500, 500, 500, 500, 1500,
    500, 9000,
};

static const char* jumper_name(JumperType type) {
    if(type == JumperAliens) return "aliens";
    if(type == JumperBees) return "bees";
    return "sheep";
}

static const char* tone_name(AlarmTone tone) {
    if(tone == ToneUrgent) return "urgent";
    if(tone == ToneRickroll) return "rickroll";
    if(tone == ToneNokia) return "nokia";
    return "classic";
}

static float tone_freq(AlarmTone tone, uint8_t step) {
    if(tone == ToneRickroll) {
        static const float notes[] = {587.0f, 659.0f, 784.0f, 659.0f, 988.0f, 988.0f, 880.0f, 740.0f};
        return notes[step & 7U];
    }
    if(tone == ToneNokia) {
        static const float notes[] = {1318.0f, 1174.0f, 740.0f, 830.0f, 1108.0f, 987.0f, 587.0f, 659.0f};
        return notes[step & 7U];
    }
    if(tone == ToneUrgent) return (step & 1U) ? 1760.0f : 988.0f;
    return (step & 1U) ? 1320.0f : 660.0f;
}

static uint8_t tone_preview_steps(AlarmTone tone) {
    if(tone == ToneRickroll || tone == ToneNokia) return 8U;
    return 6U;
}

static uint16_t tone_preview_note_ms(AlarmTone tone) {
    if(tone == ToneUrgent) return 180U;
    if(tone == ToneRickroll || tone == ToneNokia) return 220U;
    return 240U;
}

static const char* badkb_payload_name(BadkbPayload payload) {
    if(payload == BadkbCk42x) return "ck42x.com";
    if(payload == BadkbRickroll) return "rickroll";
    if(payload == BadkbWakeSearch) return "wake search";
    if(payload == BadkbBeeMusic) return "bee music";
    if(payload == BadkbWakePage) return "wakeup page";
    if(payload == BadkbLofiLive) return "lofi live";
    if(payload == BadkbNyanSearch) return "nyan search";
    if(payload == BadkbMorningAlarm) return "morning alarm";
    if(payload == BadkbTenHourTimer) return "10h timer";
    if(payload == BadkbRainSounds) return "rain sounds";
    if(payload == BadkbPomodoro) return "pomodoro";
    return "off";
}

static const char* badkb_payload_url(BadkbPayload payload) {
    if(payload == BadkbCk42x) return "https://ck42x.com";
    if(payload == BadkbRickroll) return "https://youtu.be/dQw4w9WgXcQ";
    if(payload == BadkbWakeSearch) return "https://www.youtube.com/results?search_query=alarm+clock";
    if(payload == BadkbBeeMusic) return "https://ck42x.com/beemusic";
    if(payload == BadkbWakePage) return "https://ck42x.com/tools/ck42x-wakeup/uploader";
    if(payload == BadkbLofiLive) return "https://youtu.be/EWrX250Zhko";
    if(payload == BadkbNyanSearch) return "https://www.youtube.com/results?search_query=nyan+cat+original";
    if(payload == BadkbMorningAlarm) return "https://www.youtube.com/results?search_query=morning+alarm+clock+music";
    if(payload == BadkbTenHourTimer) return "https://www.youtube.com/results?search_query=10+hour+alarm+clock+sound";
    if(payload == BadkbRainSounds) return "https://www.youtube.com/results?search_query=morning+rain+sounds";
    if(payload == BadkbPomodoro) return "https://www.youtube.com/results?search_query=study+with+me+pomodoro";
    return NULL;
}

static uint8_t menu_start_for_rows(uint8_t selected, uint8_t count, uint8_t visible_rows) {
    if(count <= visible_rows || selected < visible_rows) return 0;
    uint8_t start = (uint8_t)(selected - visible_rows + 1U);
    if(start + visible_rows > count) start = (uint8_t)(count - visible_rows);
    return start;
}

static void draw_stars(Canvas* canvas) {
    static const uint8_t stars[][2] = {
        {5,5},{120,8},{112,56},{12,49},{80,4},{96,19},{8,31},
        {124,42},{72,59},{49,7},{58,55},{28,19},{103,51},{118,16}
    };
    canvas_set_color(canvas, ColorWhite);
    for(size_t i = 0; i < sizeof(stars) / sizeof(stars[0]); i++) {
        canvas_draw_dot(canvas, stars[i][0], stars[i][1]);
    }
}

static void draw_menu_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
    uint8_t y = (uint8_t)(22U + row * 10U);
    if(selected) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 2, y - 8, 124, 10);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 6, y, text);
    canvas_set_color(canvas, ColorWhite);
}

static void draw_title(Canvas* canvas, const char* title) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 64);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 11, title);
}

static void draw_menu_list(Canvas* canvas, const char* title, const char* const* items, uint8_t count, uint8_t selected, const char* footer) {
    draw_title(canvas, title);
    uint8_t visible_rows = footer ? MENU_FOOTER_ROWS : MENU_VISIBLE_ROWS;
    uint8_t start = menu_start_for_rows(selected, count, visible_rows);
    for(uint8_t i = 0; i < visible_rows && start + i < count; i++) {
        draw_menu_row(canvas, i, (uint8_t)(start + i) == selected, items[start + i]);
    }
    if(count > visible_rows) {
        canvas_set_font(canvas, FontSecondary);
        if(start > 0) canvas_draw_str(canvas, 119, 20, "^");
        if(start + visible_rows < count) canvas_draw_str(canvas, 119, 54, "v");
    }
    if(footer) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 63, footer);
    }
}

static LevelDuration subghz_ck42x_tx_callback(void* ctx) {
    Ck42xWakeupApp* app = ctx;
    if(app->subghz_tx_index >= (sizeof(subghz_ck42x_pulse_us) / sizeof(subghz_ck42x_pulse_us[0]))) {
        return level_duration_reset();
    }
    uint16_t duration = subghz_ck42x_pulse_us[app->subghz_tx_index];
    bool level = (app->subghz_tx_index & 1U) == 0;
    app->subghz_tx_index++;
    return level_duration_make(level, duration);
}

static void service_subghz_tx(Ck42xWakeupApp* app) {
    if(app->subghz_tx_active && furi_hal_subghz_is_async_tx_complete()) {
        furi_hal_subghz_stop_async_tx();
        furi_hal_subghz_idle();
        app->subghz_tx_active = false;
    }
}

static void stop_subghz_tx(Ck42xWakeupApp* app) {
    if(app->subghz_tx_active) {
        furi_hal_subghz_stop_async_tx();
        furi_hal_subghz_idle();
        app->subghz_tx_active = false;
    }
}

static bool send_subghz_signal(Ck42xWakeupApp* app, uint8_t alarm) {
    alarm %= ALARM_COUNT;
    if(!app->alarm_subg_enabled[alarm] || app->subghz_tx_active) return false;
    if(!furi_hal_subghz_is_frequency_valid(SUBGHZ_FREQ_HZ)) {
        app->subghz_last_ok = false;
        return false;
    }

    furi_hal_subghz_idle();
    furi_hal_subghz_load_registers(subghz_device_cc1101_preset_ook_650khz_async_regs);
    app->subghz_last_freq = furi_hal_subghz_set_frequency_and_path(SUBGHZ_FREQ_HZ);
    furi_hal_subghz_flush_tx();
    app->subghz_tx_index = 0;
    app->subghz_tx_active = furi_hal_subghz_start_async_tx(subghz_ck42x_tx_callback, app);
    app->subghz_last_ok = app->subghz_tx_active;
    return app->subghz_tx_active;
}

static void badkb_type_url(const char* url) {
    if(!url || !url[0]) return;

    FuriHalUsbInterface* previous_usb = furi_hal_usb_get_config();
    bool switched_usb = false;
    if(previous_usb != &usb_hid) {
        if(!furi_hal_usb_set_config(&usb_hid, NULL)) return;
        switched_usb = true;
        furi_delay_ms(900);
    }

    if(!furi_hal_hid_is_connected()) {
        if(switched_usb && previous_usb) furi_hal_usb_set_config(previous_usb, NULL);
        return;
    }

    furi_hal_hid_kb_press(HID_KEYBOARD_R | KEY_MOD_LEFT_GUI);
    furi_delay_ms(80);
    furi_hal_hid_kb_release_all();
    furi_delay_ms(600);

    for(const char* p = url; *p; p++) {
        uint16_t key = HID_ASCII_TO_KEY(*p);
        if(key == HID_KEYBOARD_NONE) continue;
        furi_hal_hid_kb_press(key);
        furi_delay_ms(8);
        furi_hal_hid_kb_release_all();
        furi_delay_ms(4);
    }

    furi_delay_ms(200);
    furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
    furi_delay_ms(50);
    furi_hal_hid_kb_release_all();
    furi_delay_ms(250);

    if(switched_usb && previous_usb) furi_hal_usb_set_config(previous_usb, NULL);
}

static void badkb_inject_payload(BadkbPayload payload) {
    if(payload == BadkbOff) payload = BadkbBeeMusic;
    badkb_type_url(badkb_payload_url(payload));
}

static void badkb_inject_alarm(Ck42xWakeupApp* app, uint8_t alarm) {
    alarm %= ALARM_COUNT;
    if(!app->alarm_badkb_enabled[alarm]) return;
    badkb_inject_payload(app->alarm_badkb_payload[alarm]);
}

static void draw_tiny_sheep(Canvas* canvas, int16_t x, int16_t y, bool airborne) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x + 2, y - 5, 8, 5);
    canvas_draw_disc(canvas, x + 4, y - 5, 2);
    canvas_draw_disc(canvas, x + 7, y - 5, 2);
    canvas_draw_disc(canvas, x + 10, y - 4, 2);
    canvas_draw_box(canvas, x + 11, y - 4, 3, 3);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_dot(canvas, x + 13, y - 3);
    canvas_set_color(canvas, ColorWhite);

    if(airborne) {
        canvas_draw_line(canvas, x + 3, y, x + 1, y + 2);
        canvas_draw_line(canvas, x + 6, y, x + 5, y + 2);
        canvas_draw_line(canvas, x + 9, y, x + 11, y + 2);
    } else {
        canvas_draw_line(canvas, x + 3, y, x + 3, y + 3);
        canvas_draw_line(canvas, x + 6, y, x + 6, y + 3);
        canvas_draw_line(canvas, x + 9, y, x + 9, y + 3);
    }
}

static void draw_tiny_alien(Canvas* canvas, int16_t x, int16_t y, bool airborne) {
    canvas_set_color(canvas, ColorWhite);

    /* Smaller grey-style alien: teardrop head, visible arms, thin body. */
    canvas_draw_disc(canvas, x + 7, y - 10, 4);
    canvas_draw_line(canvas, x + 3, y - 7, x + 11, y - 7);
    canvas_draw_line(canvas, x + 4, y - 5, x + 10, y - 5);
    canvas_draw_line(canvas, x + 5, y - 3, x + 9, y - 3);
    canvas_draw_line(canvas, x + 6, y - 2, x + 8, y - 2);

    canvas_draw_box(canvas, x + 5, y, 5, 4);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_disc(canvas, x + 5, y - 10, 1);
    canvas_draw_disc(canvas, x + 9, y - 10, 1);
    canvas_draw_dot(canvas, x + 4, y - 9);
    canvas_draw_dot(canvas, x + 10, y - 9);
    canvas_set_color(canvas, ColorWhite);

    /* Arms plus small landing legs. */
    canvas_draw_line(canvas, x + 5, y + 1, x + 2, y + (airborne ? 0 : 2));
    canvas_draw_line(canvas, x + 9, y + 1, x + 12, y + (airborne ? 0 : 2));
    canvas_draw_line(canvas, x + 6, y + 4, x + 5, y + (airborne ? 6 : 7));
    canvas_draw_line(canvas, x + 9, y + 4, x + 10, y + (airborne ? 6 : 7));
}

static void draw_tiny_bee(Canvas* canvas, int16_t x, int16_t y, bool airborne) {
    canvas_set_color(canvas, ColorWhite);

    /* Smaller bee: compact striped body, head, wings, stinger. No legs. */
    canvas_draw_box(canvas, x + 5, y - 7, 7, 5);
    canvas_draw_disc(canvas, x + 12, y - 5, 3);
    canvas_draw_disc(canvas, x + 5, y - 5, 3);

    canvas_draw_circle(canvas, x + 7, y - 10, airborne ? 3 : 2);
    canvas_draw_circle(canvas, x + 11, y - 10, airborne ? 3 : 2);

    canvas_draw_line(canvas, x + 2, y - 5, x, y - 6);
    canvas_draw_line(canvas, x + 14, y - 7, x + 16, y - 9);
    canvas_draw_line(canvas, x + 14, y - 6, x + 16, y - 5);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, x + 6, y - 7, x + 6, y - 2);
    canvas_draw_line(canvas, x + 9, y - 7, x + 9, y - 2);
    canvas_draw_dot(canvas, x + 13, y - 5);
    canvas_set_color(canvas, ColorWhite);
}

static void draw_jumper(Canvas* canvas, Ck42xWakeupApp* app, int16_t x, int16_t y, bool airborne) {
    if(app->jumper_type == JumperAliens) {
        draw_tiny_alien(canvas, x, y, airborne);
    } else if(app->jumper_type == JumperBees) {
        draw_tiny_bee(canvas, x, y, airborne);
    } else {
        draw_tiny_sheep(canvas, x, y, airborne);
    }
}

static void draw_counter_scene(Canvas* canvas, Ck42xWakeupApp* app) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_line(canvas, 1, 51, 67, 51);

    canvas_draw_line(canvas, 35, 34, 35, 51);
    canvas_draw_line(canvas, 42, 32, 42, 51);
    canvas_draw_line(canvas, 49, 35, 49, 51);
    canvas_draw_line(canvas, 32, 39, 53, 39);
    canvas_draw_line(canvas, 32, 44, 53, 44);

    if(app->counter_enabled) {
        const uint32_t cycle_ticks = COUNTER_CYCLE_SECONDS * ANIM_TICKS_PER_SECOND;
        uint32_t phase = app->anim_tick % cycle_ticks;
        int16_t x = (int16_t)((phase * 86U) / cycle_ticks) - 16;
        int16_t distance = x > 42 ? (x - 42) : (42 - x);
        int16_t hop = 0;
        if(distance < 18) {
            hop = (int16_t)(18 - distance);
            if(hop > 13) hop = 13;
        }
        int16_t bob = (int16_t)((phase >> 1) & 1U);
        int16_t y = (int16_t)(47 - hop - bob);
        draw_jumper(canvas, app, x, y, hop > 3);
    }

    canvas_set_font(canvas, FontSecondary);
    char count_line[24];
    snprintf(count_line, sizeof(count_line), "COUNT %03u", (unsigned)(app->counter_count % 1000U));
    canvas_draw_str(canvas, 6, 13, count_line);
}

static void ir_worker_received_callback(void* ctx, InfraredWorkerSignal* signal) {
    Ck42xWakeupApp* app = ctx;
    if(!app->ir_recording || app->ir_capture_done) return;
    if(infrared_worker_signal_is_decoded(signal)) return;

    const uint32_t* timings = NULL;
    size_t count = 0;
    infrared_worker_get_raw_signal(signal, &timings, &count);
    if(!timings || count < 8U) return;

    if(count > IR_MAX_TIMINGS) count = IR_MAX_TIMINGS;
    uint8_t alarm = app->ir_record_alarm % ALARM_COUNT;
    for(size_t i = 0; i < count; i++) {
        app->ir_timings[alarm][i] = timings[i];
    }
    app->ir_count[alarm] = (uint16_t)count;
    app->ir_start_from_mark[alarm] = true;
    app->ir_ready[alarm] = true;
    app->ir_capture_done = true;
}

static void stop_ir_capture(Ck42xWakeupApp* app) {
    if(app->ir_worker) {
        infrared_worker_rx_stop(app->ir_worker);
        infrared_worker_free(app->ir_worker);
        app->ir_worker = NULL;
    }
    app->ir_recording = false;
}

static void start_ir_capture(Ck42xWakeupApp* app, uint8_t alarm) {
    if(app->ir_recording) stop_ir_capture(app);
    if(furi_hal_infrared_is_busy()) return;
    alarm %= ALARM_COUNT;
    app->ir_record_alarm = alarm;
    app->ir_count[alarm] = 0;
    app->ir_ready[alarm] = false;
    app->ir_capture_done = false;
    app->ir_recording = true;
    app->screen = ScreenIrCapture;
    app->ir_worker = infrared_worker_alloc();
    infrared_worker_rx_enable_signal_decoding(app->ir_worker, false);
    infrared_worker_rx_enable_blink_on_receiving(app->ir_worker, true);
    infrared_worker_rx_set_received_signal_callback(app->ir_worker, ir_worker_received_callback, app);
    infrared_worker_rx_start(app->ir_worker);
}

static void send_ir_signal(Ck42xWakeupApp* app, uint8_t alarm) {
    alarm %= ALARM_COUNT;
    uint16_t count = app->ir_count[alarm];
    if(!app->alarm_ir_enabled[alarm] || !app->ir_ready[alarm] || count < 8U || furi_hal_infrared_is_busy()) return;
    infrared_send_raw_ext(app->ir_timings[alarm], count, app->ir_start_from_mark[alarm], 38000UL, 0.33f);
}

static void start_alarm_audio(Ck42xWakeupApp* app, uint8_t alarm) {
    alarm %= ALARM_COUNT;
    if(!app->alarm_audio_enabled[alarm]) return;
    if(furi_hal_speaker_acquire(100)) {
        furi_hal_speaker_start(tone_freq(app->alarm_tone[alarm], 0), 0.55f);
    }
}

static void stop_alarm_audio(void) {
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

static void test_tone(AlarmTone tone) {
    if(furi_hal_speaker_acquire(100)) {
        uint8_t steps = tone_preview_steps(tone);
        uint16_t note_ms = tone_preview_note_ms(tone);
        for(uint8_t i = 0; i < steps; i++) {
            furi_hal_speaker_start(tone_freq(tone, i), 0.45f);
            furi_delay_ms(note_ms);
            furi_hal_speaker_stop();
            furi_delay_ms(30);
        }
        furi_hal_speaker_release();
    }
}

static void counter_save(Ck42xWakeupApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, COUNTER_SAVE_DIR);
    File* file = storage_file_alloc(storage);
    CounterSave save = {
        .magic = COUNTER_SAVE_MAGIC,
        .version = COUNTER_SAVE_VERSION,
        .counter_count = app->counter_count,
    };
    if(storage_file_open(file, COUNTER_SAVE_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &save, sizeof(save));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void counter_load(Ck42xWakeupApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, COUNTER_SAVE_DIR);
    File* file = storage_file_alloc(storage);
    CounterSave save = {0};
    if(storage_file_open(file, COUNTER_SAVE_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_size(file) == sizeof(save)) {
            storage_file_read(file, &save, sizeof(save));
            if(save.magic == COUNTER_SAVE_MAGIC && save.version == COUNTER_SAVE_VERSION) {
                app->counter_count = save.counter_count;
            }
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void alarm_save(Ck42xWakeupApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, COUNTER_SAVE_DIR);
    File* file = storage_file_alloc(storage);
    AlarmSave* save = malloc(sizeof(AlarmSave));
    if(save) {
        memset(save, 0, sizeof(AlarmSave));
        save->magic = ALARM_SAVE_MAGIC;
        save->version = ALARM_SAVE_VERSION;
        save->size = (uint16_t)sizeof(AlarmSave);
        for(uint8_t i = 0; i < ALARM_COUNT; i++) {
            save->configured[i] = app->alarm_configured[i] ? 1U : 0U;
            save->enabled[i] = app->alarm_enabled[i] ? 1U : 0U;
            save->hour[i] = app->alarm_hr[i];
            save->minute[i] = app->alarm_min[i];
            save->ir_enabled[i] = app->alarm_ir_enabled[i] ? 1U : 0U;
            save->subg_enabled[i] = app->alarm_subg_enabled[i] ? 1U : 0U;
            save->haptic_enabled[i] = app->alarm_haptic_enabled[i] ? 1U : 0U;
            save->audio_enabled[i] = app->alarm_audio_enabled[i] ? 1U : 0U;
            save->tone[i] = (uint8_t)app->alarm_tone[i];
            save->badkb_enabled[i] = app->alarm_badkb_enabled[i] ? 1U : 0U;
            save->badkb_payload[i] = (uint8_t)app->alarm_badkb_payload[i];
            save->ir_ready[i] = app->ir_ready[i] ? 1U : 0U;
            save->ir_start_from_mark[i] = app->ir_start_from_mark[i] ? 1U : 0U;
            save->ir_count[i] = app->ir_count[i] <= IR_MAX_TIMINGS ? app->ir_count[i] : IR_MAX_TIMINGS;
            memcpy(save->ir_timings[i], app->ir_timings[i], sizeof(save->ir_timings[i]));
        }
        if(storage_file_open(file, ALARM_SAVE_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(file, save, sizeof(AlarmSave));
            storage_file_close(file);
        }
        free(save);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void alarm_load(Ck42xWakeupApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, COUNTER_SAVE_DIR);
    File* file = storage_file_alloc(storage);
    AlarmSave* save = malloc(sizeof(AlarmSave));
    if(save) {
        memset(save, 0, sizeof(AlarmSave));
        if(storage_file_open(file, ALARM_SAVE_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
            if(storage_file_size(file) == sizeof(AlarmSave)) {
                storage_file_read(file, save, sizeof(AlarmSave));
                if(save->magic == ALARM_SAVE_MAGIC && save->version == ALARM_SAVE_VERSION && save->size == sizeof(AlarmSave)) {
                    for(uint8_t i = 0; i < ALARM_COUNT; i++) {
                        app->alarm_configured[i] = save->configured[i] != 0U;
                        app->alarm_enabled[i] = save->enabled[i] != 0U;
                        app->alarm_hr[i] = save->hour[i] < 24U ? save->hour[i] : 0U;
                        app->alarm_min[i] = save->minute[i] < 60U ? save->minute[i] : 0U;
                        app->alarm_ir_enabled[i] = save->ir_enabled[i] != 0U;
                        app->alarm_subg_enabled[i] = save->subg_enabled[i] != 0U;
                        app->alarm_haptic_enabled[i] = save->haptic_enabled[i] != 0U;
                        app->alarm_audio_enabled[i] = save->audio_enabled[i] != 0U;
                        app->alarm_tone[i] = save->tone[i] < ToneCount ? (AlarmTone)save->tone[i] : ToneClassic;
                        app->alarm_badkb_enabled[i] = save->badkb_enabled[i] != 0U;
                        app->alarm_badkb_payload[i] = save->badkb_payload[i] < BadkbCount ? (BadkbPayload)save->badkb_payload[i] : BadkbOff;
                        app->ir_ready[i] = save->ir_ready[i] != 0U;
                        app->ir_start_from_mark[i] = save->ir_start_from_mark[i] != 0U;
                        app->ir_count[i] = save->ir_count[i] <= IR_MAX_TIMINGS ? save->ir_count[i] : IR_MAX_TIMINGS;
                        memcpy(app->ir_timings[i], save->ir_timings[i], sizeof(app->ir_timings[i]));
                        if(!app->ir_ready[i]) {
                            app->alarm_ir_enabled[i] = false;
                            app->ir_count[i] = 0;
                        }
                    }
                }
            }
            storage_file_close(file);
        }
        free(save);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void backlight_apply(Ck42xWakeupApp* app) {
    if(!app->notification) return;
    notification_message(
        app->notification,
        app->backlight_keep_on ? &sequence_display_backlight_enforce_on : &sequence_display_backlight_enforce_auto);
}

static void prefs_save(Ck42xWakeupApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, COUNTER_SAVE_DIR);
    File* file = storage_file_alloc(storage);
    PrefsSave save = {
        .magic = PREFS_SAVE_MAGIC,
        .version = PREFS_SAVE_VERSION,
        .backlight_keep_on = app->backlight_keep_on ? 1U : 0U,
        .reserved = 0U,
    };
    if(storage_file_open(file, PREFS_SAVE_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &save, sizeof(save));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void prefs_load(Ck42xWakeupApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, COUNTER_SAVE_DIR);
    File* file = storage_file_alloc(storage);
    PrefsSave save = {0};
    if(storage_file_open(file, PREFS_SAVE_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_size(file) == sizeof(save)) {
            storage_file_read(file, &save, sizeof(save));
            if(save.magic == PREFS_SAVE_MAGIC && save.version == PREFS_SAVE_VERSION) {
                app->backlight_keep_on = save.backlight_keep_on != 0U;
            }
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void enter_ring(Ck42xWakeupApp* app, uint8_t alarm_index) {
    app->ringing_alarm = (uint8_t)(alarm_index % ALARM_COUNT);
    app->screen = ScreenRing;
    app->ring_pulse = 0;
    if(app->alarm_haptic_enabled[app->ringing_alarm]) furi_hal_vibro_on(true);
    start_alarm_audio(app, app->ringing_alarm);
    send_ir_signal(app, app->ringing_alarm);
    send_subghz_signal(app, app->ringing_alarm);
    badkb_inject_alarm(app, app->ringing_alarm);
}

static uint32_t ck42x_time_fields_to_seconds(uint8_t hour, uint8_t minute, uint8_t second) {
    return (uint32_t)hour * 3600U + (uint32_t)minute * 60U + (uint32_t)second;
}

static uint32_t ck42x_build_time_seconds(void) {
    uint8_t hour = (uint8_t)((__TIME__[0] - '0') * 10 + (__TIME__[1] - '0'));
    uint8_t minute = (uint8_t)((__TIME__[3] - '0') * 10 + (__TIME__[4] - '0'));
    uint8_t second = (uint8_t)((__TIME__[6] - '0') * 10 + (__TIME__[7] - '0'));
    return ck42x_time_fields_to_seconds(hour, minute, second);
}

static bool ck42x_rtc_time_seconds(uint32_t* seconds) {
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);

    if(datetime.year < 2024U || datetime.year > 2099U) return false;
    if(datetime.month < 1U || datetime.month > 12U) return false;
    if(datetime.day < 1U || datetime.day > 31U) return false;
    if(datetime.hour > 23U || datetime.minute > 59U || datetime.second > 59U) return false;

    *seconds = ck42x_time_fields_to_seconds(datetime.hour, datetime.minute, datetime.second);
    return true;
}

static uint32_t ck42x_initial_time_seconds(void) {
    uint32_t seconds = 0;
    if(ck42x_rtc_time_seconds(&seconds)) return seconds;
    return ck42x_build_time_seconds();
}

static void ck42x_clock_sync_fields(Ck42xWakeupApp* app) {
    uint32_t total = app->seconds;
    app->sec = total % 60U;
    total /= 60U;
    app->min = total % 60U;
    total /= 60U;
    app->hr = total % 24U;
}

static void ck42x_clock_tick(Ck42xWakeupApp* app) {
    app->seconds++;
    ck42x_clock_sync_fields(app);
    app->jumper_frame = (uint8_t)(app->seconds % COUNTER_CYCLE_SECONDS);
    if(app->counter_enabled && (app->seconds % COUNTER_CYCLE_SECONDS) == 0U) {
        app->counter_count++;
    }

    if(app->sec != 0) {
        for(uint8_t i = 0; i < ALARM_COUNT; i++) {
            app->alarm_fired_this_minute[i] = false;
        }
    }
    if(app->screen != ScreenRing && app->sec == 0) {
        for(uint8_t i = 0; i < ALARM_COUNT; i++) {
            if(app->alarm_configured[i] && app->alarm_enabled[i] && !app->alarm_fired_this_minute[i] && app->hr == app->alarm_hr[i] && app->min == app->alarm_min[i]) {
                app->alarm_fired_this_minute[i] = true;
                enter_ring(app, i);
                break;
            }
        }
    }
    if(app->screen == ScreenRing) {
        uint8_t alarm = app->ringing_alarm;
        app->ring_pulse = (uint8_t)((app->ring_pulse + 1U) & 3U);
        furi_hal_vibro_on(app->alarm_haptic_enabled[alarm] && ((app->ring_pulse & 1U) != 0));
        if(app->alarm_audio_enabled[alarm] && furi_hal_speaker_is_mine()) {
            furi_hal_speaker_start(tone_freq(app->alarm_tone[alarm], app->ring_pulse), 0.55f);
        }
    }
}

static void draw_main(Canvas* canvas, Ck42xWakeupApp* app) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 64);
    draw_stars(canvas);
    draw_counter_scene(canvas, app);

    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 67, 11, "CK42X");

    canvas_set_font(canvas, FontBigNumbers);
    char time_line[16];
    snprintf(time_line, sizeof(time_line), "%02u:%02u", (unsigned)app->hr, (unsigned)app->min);
    canvas_draw_str(canvas, 61, 29, time_line);

    canvas_set_font(canvas, FontSecondary);
    char line[32];
    snprintf(line, sizeof(line), "TIME %02u:%02u", (unsigned)app->hr, (unsigned)app->min);
    canvas_draw_str(canvas, 67, 42, line);
    uint8_t sel = app->selected_alarm;
    if(app->alarm_configured[sel]) {
        snprintf(line, sizeof(line), "A%u %s %02u:%02u", (unsigned)(sel + 1U), app->alarm_enabled[sel] ? "ON" : "off", (unsigned)app->alarm_hr[sel], (unsigned)app->alarm_min[sel]);
        canvas_draw_str(canvas, 67, 53, line);
        snprintf(line, sizeof(line), "IR%s SG%s", app->ir_ready[sel] ? "+" : "-", app->alarm_subg_enabled[sel] ? "+" : "-");
        canvas_draw_str(canvas, 67, 63, line);
    } else {
        canvas_draw_str(canvas, 67, 53, "No alarms");
    }
    canvas_draw_str(canvas, 2, 63, "OK menu");
}

static void draw_menu(Canvas* canvas, Ck42xWakeupApp* app) {
    static const char* const items[] = {"Alarms", "About", "Settings"};
    draw_menu_list(canvas, "CK42X WakeUp", items, 3U, app->menu_cursor, "OK select | Back home");
}

static uint8_t configured_alarm_count(Ck42xWakeupApp* app) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < ALARM_COUNT; i++) {
        if(app->alarm_configured[i]) count++;
    }
    return count;
}

static uint8_t alarm_index_for_list_pos(Ck42xWakeupApp* app, uint8_t pos) {
    uint8_t seen = 0;
    for(uint8_t i = 0; i < ALARM_COUNT; i++) {
        if(app->alarm_configured[i]) {
            if(seen == pos) return i;
            seen++;
        }
    }
    return ALARM_COUNT;
}

static void draw_alarm_list(Canvas* canvas, Ck42xWakeupApp* app) {
    draw_title(canvas, "Alarms");
    uint8_t configured = configured_alarm_count(app);
    uint8_t count = (uint8_t)(configured + 1U);
    if(app->alarm_list_cursor >= count) app->alarm_list_cursor = (uint8_t)(count - 1U);
    uint8_t start = menu_start_for_rows(app->alarm_list_cursor, count, MENU_FOOTER_ROWS);
    for(uint8_t row = 0; row < MENU_FOOTER_ROWS && start + row < count; row++) {
        uint8_t pos = (uint8_t)(start + row);
        uint8_t idx = alarm_index_for_list_pos(app, pos);
        char line[40];
        if(idx < ALARM_COUNT) {
            snprintf(line, sizeof(line), "A%u %s %02u:%02u IR%s SG%s", (unsigned)(idx + 1U), app->alarm_enabled[idx] ? "ON " : "off", (unsigned)app->alarm_hr[idx], (unsigned)app->alarm_min[idx], app->ir_ready[idx] ? "+" : "-", app->alarm_subg_enabled[idx] ? "+" : "-");
        } else {
            snprintf(line, sizeof(line), "+ new alarm");
        }
        draw_menu_row(canvas, row, pos == app->alarm_list_cursor, line);
    }
    if(configured == 0U) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 8, 48, "No alarms yet");
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 63, "OK edit/add | Back menu");
}

static void draw_alarm_detail(Canvas* canvas, Ck42xWakeupApp* app) {
    draw_title(canvas, "Alarm Config");
    uint8_t sel = app->selected_alarm;
    char rows[9][40];
    if(app->detail_cursor >= 9U) app->detail_cursor = 8U;
    snprintf(rows[0], sizeof(rows[0]), "Time       %02u:%02u", (unsigned)app->alarm_hr[sel], (unsigned)app->alarm_min[sel]);
    snprintf(rows[1], sizeof(rows[1]), "Enabled    %s", app->alarm_enabled[sel] ? "ON" : "OFF");
    snprintf(rows[2], sizeof(rows[2]), "IR alarm   %s %03u", app->alarm_ir_enabled[sel] ? "ON" : "OFF", (unsigned)app->ir_count[sel]);
    snprintf(rows[3], sizeof(rows[3]), "SubG       %s", app->alarm_subg_enabled[sel] ? "ON" : "OFF");
    snprintf(rows[4], sizeof(rows[4]), "Haptics    %s", app->alarm_haptic_enabled[sel] ? "ON" : "OFF");
    snprintf(rows[5], sizeof(rows[5]), "Audio      %s %s", app->alarm_audio_enabled[sel] ? "ON" : "OFF", tone_name(app->alarm_tone[sel]));
    snprintf(rows[6], sizeof(rows[6]), "BadKB      %s", app->alarm_badkb_enabled[sel] ? badkb_payload_name(app->alarm_badkb_payload[sel]) : "OFF");
    snprintf(rows[7], sizeof(rows[7]), "Test alarm now");
    snprintf(rows[8], sizeof(rows[8]), "Delete/clear slot");
    uint8_t start = menu_start_for_rows(app->detail_cursor, 9U, MENU_FOOTER_ROWS);
    for(uint8_t row = 0; row < MENU_FOOTER_ROWS && start + row < 9U; row++) {
        draw_menu_row(canvas, row, (uint8_t)(start + row) == app->detail_cursor, rows[start + row]);
    }
    canvas_set_font(canvas, FontSecondary);
    char footer[32];
    snprintf(footer, sizeof(footer), "A%u OK | Back list", (unsigned)(sel + 1U));
    canvas_draw_str(canvas, 4, 63, footer);
}

static void draw_time_select(Canvas* canvas, Ck42xWakeupApp* app) {
    draw_title(canvas, "Time Selector");
    uint8_t sel = app->selected_alarm;
    canvas_set_font(canvas, FontBigNumbers);
    char line[16];
    snprintf(line, sizeof(line), "%02u:%02u", (unsigned)app->alarm_hr[sel], (unsigned)app->alarm_min[sel]);
    canvas_draw_str(canvas, 34, 35, line);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, app->time_field == 0 ? 42 : 75, 45, "^^");
    canvas_draw_str(canvas, 8, 57, app->time_field == 0 ? "Hour selected" : "Minute selected");
    canvas_draw_str(canvas, 4, 63, "Up/Dn edit L/R field OK save");
}

static void draw_ir_menu(Canvas* canvas, Ck42xWakeupApp* app) {
    uint8_t sel = app->selected_alarm;
    char rows[4][40];
    snprintf(rows[0], sizeof(rows[0]), "Capture remote");
    snprintf(rows[1], sizeof(rows[1]), "Enabled %s", app->alarm_ir_enabled[sel] ? "ON" : "OFF");
    snprintf(rows[2], sizeof(rows[2]), "Test replay %03u", (unsigned)app->ir_count[sel]);
    snprintf(rows[3], sizeof(rows[3]), "Save/back RAM");
    const char* ptrs[4] = {rows[0], rows[1], rows[2], rows[3]};
    draw_menu_list(canvas, "IR Alarm", ptrs, 4U, app->ir_menu_cursor, "Capture saves to alarm RAM");
}

static void draw_ir_capture(Canvas* canvas, Ck42xWakeupApp* app) {
    draw_title(canvas, "CK42X IR Capture");
    canvas_set_font(canvas, FontSecondary);
    char line[40];
    uint8_t alarm = app->ir_record_alarm % ALARM_COUNT;
    snprintf(line, sizeof(line), "A%u: %s", (unsigned)(alarm + 1U), app->ir_recording ? "listening" : (app->ir_ready[alarm] ? "captured" : "empty"));
    canvas_draw_str(canvas, 6, 27, line);
    snprintf(line, sizeof(line), "Timings: %03u/%u", (unsigned)app->ir_count[alarm], (unsigned)IR_MAX_TIMINGS);
    canvas_draw_str(canvas, 6, 39, line);
    canvas_draw_str(canvas, 6, 51, "Aim remote, press key");
    canvas_draw_str(canvas, 6, 63, "OK retry Rt test Back save");
}

static void draw_subg_menu(Canvas* canvas, Ck42xWakeupApp* app) {
    uint8_t sel = app->selected_alarm;
    char rows[4][40];
    snprintf(rows[0], sizeof(rows[0]), "Capture/learn TBD");
    snprintf(rows[1], sizeof(rows[1]), "Enabled %s", app->alarm_subg_enabled[sel] ? "ON" : "OFF");
    snprintf(rows[2], sizeof(rows[2]), "Test 433.92 TX");
    snprintf(rows[3], sizeof(rows[3]), "Save/back");
    const char* ptrs[4] = {rows[0], rows[1], rows[2], rows[3]};
    draw_menu_list(canvas, "Sub-GHz Alarm", ptrs, 4U, app->subg_menu_cursor, "Learn blocked; TX works");
}

static void draw_badkb_menu(Canvas* canvas, Ck42xWakeupApp* app) {
    uint8_t sel = app->selected_alarm;
    char rows[4][40];
    snprintf(rows[0], sizeof(rows[0]), "Enabled %s", app->alarm_badkb_enabled[sel] ? "ON" : "OFF");
    snprintf(rows[1], sizeof(rows[1]), "Payload %s", badkb_payload_name(app->alarm_badkb_payload[sel]));
    snprintf(rows[2], sizeof(rows[2]), "Test payload");
    snprintf(rows[3], sizeof(rows[3]), "Save/back");
    const char* ptrs[4] = {rows[0], rows[1], rows[2], rows[3]};
    draw_menu_list(canvas, "BadKB", ptrs, 4U, app->badkb_menu_cursor, "Built-in URL payloads");
}

static void draw_audio_menu(Canvas* canvas, Ck42xWakeupApp* app) {
    uint8_t sel = app->selected_alarm;
    char rows[4][40];
    snprintf(rows[0], sizeof(rows[0]), "Enabled %s", app->alarm_audio_enabled[sel] ? "ON" : "OFF");
    snprintf(rows[1], sizeof(rows[1]), "Tone %s", tone_name(app->alarm_tone[sel]));
    snprintf(rows[2], sizeof(rows[2]), "Test tone");
    snprintf(rows[3], sizeof(rows[3]), "Save/back");
    const char* ptrs[4] = {rows[0], rows[1], rows[2], rows[3]};
    draw_menu_list(canvas, "Audio", ptrs, 4U, app->audio_menu_cursor, "Classic/urgent/rick/nokia");
}

static void draw_counter_menu(Canvas* canvas, Ck42xWakeupApp* app) {
    char rows[4][40];
    snprintf(rows[0], sizeof(rows[0]), "Enabled %s", app->counter_enabled ? "ON" : "OFF");
    snprintf(rows[1], sizeof(rows[1]), "Character %s", jumper_name(app->jumper_type));
    snprintf(rows[2], sizeof(rows[2]), "Reset count %03u", (unsigned)(app->counter_count % 1000U));
    snprintf(rows[3], sizeof(rows[3]), "Save/back");
    const char* ptrs[4] = {rows[0], rows[1], rows[2], rows[3]};
    draw_menu_list(canvas, "Character", ptrs, 4U, app->counter_menu_cursor, "Global setting saves");
}

static void draw_about(Canvas* canvas) {
    draw_title(canvas, "About");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 6, 23, "_CK42X WakeUp v2.29");
    canvas_draw_str(canvas, 6, 34, "manual-loop alarm lab");
    canvas_draw_str(canvas, 6, 45, "IR Worker / SubG TX");
    canvas_draw_str(canvas, 6, 56, "Alarm+counter save");
    canvas_draw_str(canvas, 4, 63, "Back menu");
}

static void draw_settings(Canvas* canvas, Ck42xWakeupApp* app) {
    char rows[5][40];
    snprintf(rows[0], sizeof(rows[0]), "Clock +1 minute");
    snprintf(rows[1], sizeof(rows[1]), "Clock -1 minute");
    snprintf(rows[2], sizeof(rows[2]), "Backlight keep %s", app->backlight_keep_on ? "ON" : "OFF");
    snprintf(rows[3], sizeof(rows[3]), "Character %s", app->counter_enabled ? jumper_name(app->jumper_type) : "OFF");
    snprintf(rows[4], sizeof(rows[4]), "Back");
    const char* ptrs[5] = {rows[0], rows[1], rows[2], rows[3], rows[4]};
    draw_menu_list(canvas, "Settings", ptrs, 5U, app->settings_cursor, "Backlight persists");
}

static void draw_ring(Canvas* canvas, Ck42xWakeupApp* app) {
    canvas_set_color(canvas, app->ring_pulse & 1U ? ColorWhite : ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 64);
    canvas_set_color(canvas, app->ring_pulse & 1U ? ColorBlack : ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 8, 13, "CK42X WAKEUP");
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, 22, 38, "ALARM");
    canvas_set_font(canvas, FontSecondary);
    char line[40];
    uint8_t alarm = app->ringing_alarm;
    snprintf(line, sizeof(line), "A%u %02u:%02u %s", (unsigned)(alarm + 1U), (unsigned)app->alarm_hr[alarm], (unsigned)app->alarm_min[alarm], tone_name(app->alarm_tone[alarm]));
    canvas_draw_str(canvas, 15, 51, line);
    canvas_draw_str(canvas, 5, 63, "OK dismiss | vibe sound IR SG BK");
}

static void ck42x_draw_callback(Canvas* canvas, void* context) {
    Ck42xWakeupApp* app = context;
    canvas_clear(canvas);
    if(app->screen == ScreenMenu) draw_menu(canvas, app);
    else if(app->screen == ScreenAlarmList) draw_alarm_list(canvas, app);
    else if(app->screen == ScreenAlarmDetail) draw_alarm_detail(canvas, app);
    else if(app->screen == ScreenTimeSelect) draw_time_select(canvas, app);
    else if(app->screen == ScreenIrMenu) draw_ir_menu(canvas, app);
    else if(app->screen == ScreenIrCapture) draw_ir_capture(canvas, app);
    else if(app->screen == ScreenSubgMenu) draw_subg_menu(canvas, app);
    else if(app->screen == ScreenBadkbMenu) draw_badkb_menu(canvas, app);
    else if(app->screen == ScreenAudioMenu) draw_audio_menu(canvas, app);
    else if(app->screen == ScreenCounterMenu) draw_counter_menu(canvas, app);
    else if(app->screen == ScreenAbout) draw_about(canvas);
    else if(app->screen == ScreenSettings) draw_settings(canvas, app);
    else if(app->screen == ScreenRing) draw_ring(canvas, app);
    else draw_main(canvas, app);
}

static void cursor_up(uint8_t* cursor, uint8_t count) {
    *cursor = (*cursor == 0U) ? (uint8_t)(count - 1U) : (uint8_t)(*cursor - 1U);
}

static void cursor_down(uint8_t* cursor, uint8_t count) {
    *cursor = (uint8_t)((*cursor + 1U) % count);
}

static uint8_t first_empty_alarm(Ck42xWakeupApp* app) {
    for(uint8_t i = 0; i < ALARM_COUNT; i++) {
        if(!app->alarm_configured[i]) return i;
    }
    return app->selected_alarm;
}

static void clear_alarm(Ck42xWakeupApp* app, uint8_t alarm) {
    alarm %= ALARM_COUNT;
    app->alarm_configured[alarm] = false;
    app->alarm_enabled[alarm] = false;
    app->alarm_hr[alarm] = 0;
    app->alarm_min[alarm] = 0;
    app->alarm_ir_enabled[alarm] = false;
    app->alarm_subg_enabled[alarm] = false;
    app->alarm_haptic_enabled[alarm] = true;
    app->alarm_audio_enabled[alarm] = true;
    app->alarm_tone[alarm] = ToneClassic;
    app->alarm_badkb_enabled[alarm] = false;
    app->alarm_badkb_payload[alarm] = BadkbOff;
    app->ir_ready[alarm] = false;
    app->ir_count[alarm] = 0;
}

static void handle_main_input(Ck42xWakeupApp* app, InputKey key) {
    if(key == InputKeyBack) app->running = false;
    else if(key == InputKeyOk) app->screen = ScreenMenu;
    else if(key == InputKeyUp) enter_ring(app, app->selected_alarm);
}

static void handle_menu_input(Ck42xWakeupApp* app, InputKey key) {
    if(key == InputKeyBack) app->screen = ScreenMain;
    else if(key == InputKeyUp) cursor_up(&app->menu_cursor, 3U);
    else if(key == InputKeyDown) cursor_down(&app->menu_cursor, 3U);
    else if(key == InputKeyOk) {
        if(app->menu_cursor == 0U) app->screen = ScreenAlarmList;
        else if(app->menu_cursor == 1U) app->screen = ScreenAbout;
        else app->screen = ScreenSettings;
    }
}

static void handle_alarm_list_input(Ck42xWakeupApp* app, InputKey key) {
    uint8_t count = (uint8_t)(configured_alarm_count(app) + 1U);
    if(app->alarm_list_cursor >= count) app->alarm_list_cursor = (uint8_t)(count - 1U);
    if(key == InputKeyBack) app->screen = ScreenMenu;
    else if(key == InputKeyUp) cursor_up(&app->alarm_list_cursor, count);
    else if(key == InputKeyDown) cursor_down(&app->alarm_list_cursor, count);
    else if(key == InputKeyOk) {
        uint8_t idx = alarm_index_for_list_pos(app, app->alarm_list_cursor);
        if(idx >= ALARM_COUNT) {
            app->selected_alarm = first_empty_alarm(app);
            app->alarm_configured[app->selected_alarm] = true;
            app->alarm_enabled[app->selected_alarm] = false;
            alarm_save(app);
        } else {
            app->selected_alarm = idx;
        }
        app->detail_cursor = 0;
        app->screen = ScreenAlarmDetail;
    }
}

static void handle_alarm_detail_input(Ck42xWakeupApp* app, InputKey key) {
    uint8_t sel = app->selected_alarm;
    if(app->detail_cursor >= 9U) app->detail_cursor = 8U;
    if(key == InputKeyBack) {
        alarm_save(app);
        app->screen = ScreenAlarmList;
    } else if(key == InputKeyUp) cursor_up(&app->detail_cursor, 9U);
    else if(key == InputKeyDown) cursor_down(&app->detail_cursor, 9U);
    else if(key == InputKeyOk) {
        if(app->detail_cursor == 0U) app->screen = ScreenTimeSelect;
        else if(app->detail_cursor == 1U) {
            app->alarm_enabled[sel] = !app->alarm_enabled[sel];
            alarm_save(app);
        } else if(app->detail_cursor == 2U) app->screen = ScreenIrMenu;
        else if(app->detail_cursor == 3U) app->screen = ScreenSubgMenu;
        else if(app->detail_cursor == 4U) {
            app->alarm_haptic_enabled[sel] = !app->alarm_haptic_enabled[sel];
            alarm_save(app);
        } else if(app->detail_cursor == 5U) app->screen = ScreenAudioMenu;
        else if(app->detail_cursor == 6U) app->screen = ScreenBadkbMenu;
        else if(app->detail_cursor == 7U) enter_ring(app, sel);
        else if(app->detail_cursor == 8U) {
            clear_alarm(app, sel);
            alarm_save(app);
        }
    }
}

static void handle_time_input(Ck42xWakeupApp* app, InputKey key) {
    uint8_t sel = app->selected_alarm;
    if(key == InputKeyBack || key == InputKeyOk) {
        app->alarm_configured[sel] = true;
        app->alarm_enabled[sel] = true;
        alarm_save(app);
        app->screen = ScreenAlarmDetail;
    } else if(key == InputKeyLeft || key == InputKeyRight) {
        app->time_field = (uint8_t)(1U - app->time_field);
    } else if(key == InputKeyUp || key == InputKeyDown) {
        bool up = key == InputKeyUp;
        if(app->time_field == 0U) {
            app->alarm_hr[sel] = up ? (uint8_t)((app->alarm_hr[sel] + 1U) % 24U) : (uint8_t)((app->alarm_hr[sel] + 23U) % 24U);
        } else {
            app->alarm_min[sel] = up ? (uint8_t)((app->alarm_min[sel] + 1U) % 60U) : (uint8_t)((app->alarm_min[sel] + 59U) % 60U);
        }
    }
}

static void handle_ir_menu_input(Ck42xWakeupApp* app, InputKey key) {
    uint8_t sel = app->selected_alarm;
    if(key == InputKeyBack) {
        alarm_save(app);
        app->screen = ScreenAlarmDetail;
    } else if(key == InputKeyUp) cursor_up(&app->ir_menu_cursor, 4U);
    else if(key == InputKeyDown) cursor_down(&app->ir_menu_cursor, 4U);
    else if(key == InputKeyOk) {
        if(app->ir_menu_cursor == 0U) start_ir_capture(app, sel);
        else if(app->ir_menu_cursor == 1U) {
            app->alarm_ir_enabled[sel] = !app->alarm_ir_enabled[sel];
            alarm_save(app);
        } else if(app->ir_menu_cursor == 2U) send_ir_signal(app, sel);
        else {
            alarm_save(app);
            app->screen = ScreenAlarmDetail;
        }
    }
}

static void handle_ir_capture_input(Ck42xWakeupApp* app, InputKey key) {
    uint8_t alarm = app->ir_record_alarm % ALARM_COUNT;
    if(key == InputKeyBack) {
        stop_ir_capture(app);
        app->alarm_ir_enabled[alarm] = app->ir_ready[alarm];
        alarm_save(app);
        app->screen = ScreenIrMenu;
    } else if(key == InputKeyOk) {
        start_ir_capture(app, alarm);
    } else if(key == InputKeyRight) {
        stop_ir_capture(app);
        app->alarm_ir_enabled[alarm] = app->ir_ready[alarm];
        alarm_save(app);
        send_ir_signal(app, alarm);
    }
}

static void handle_subg_menu_input(Ck42xWakeupApp* app, InputKey key) {
    uint8_t sel = app->selected_alarm;
    if(key == InputKeyBack) {
        alarm_save(app);
        app->screen = ScreenAlarmDetail;
    } else if(key == InputKeyUp) cursor_up(&app->subg_menu_cursor, 4U);
    else if(key == InputKeyDown) cursor_down(&app->subg_menu_cursor, 4U);
    else if(key == InputKeyOk) {
        if(app->subg_menu_cursor == 0U) {
            app->alarm_subg_enabled[sel] = false;
            alarm_save(app);
        } else if(app->subg_menu_cursor == 1U) {
            app->alarm_subg_enabled[sel] = !app->alarm_subg_enabled[sel];
            alarm_save(app);
        } else if(app->subg_menu_cursor == 2U) {
            send_subghz_signal(app, sel);
        } else {
            alarm_save(app);
            app->screen = ScreenAlarmDetail;
        }
    }
}

static void handle_badkb_menu_input(Ck42xWakeupApp* app, InputKey key) {
    uint8_t sel = app->selected_alarm;
    if(key == InputKeyBack) {
        alarm_save(app);
        app->screen = ScreenAlarmDetail;
    } else if(key == InputKeyUp) cursor_up(&app->badkb_menu_cursor, 4U);
    else if(key == InputKeyDown) cursor_down(&app->badkb_menu_cursor, 4U);
    else if(key == InputKeyOk) {
        if(app->badkb_menu_cursor == 0U) {
            app->alarm_badkb_enabled[sel] = !app->alarm_badkb_enabled[sel];
            if(app->alarm_badkb_enabled[sel] && app->alarm_badkb_payload[sel] == BadkbOff) {
                app->alarm_badkb_payload[sel] = BadkbBeeMusic;
            }
            alarm_save(app);
        } else if(app->badkb_menu_cursor == 1U) {
            app->alarm_badkb_payload[sel] = (BadkbPayload)((app->alarm_badkb_payload[sel] + 1U) % BadkbCount);
            alarm_save(app);
        } else if(app->badkb_menu_cursor == 2U) badkb_inject_payload(app->alarm_badkb_payload[sel]);
        else {
            alarm_save(app);
            app->screen = ScreenAlarmDetail;
        }
    }
}

static void handle_audio_menu_input(Ck42xWakeupApp* app, InputKey key) {
    uint8_t sel = app->selected_alarm;
    if(key == InputKeyBack) {
        alarm_save(app);
        app->screen = ScreenAlarmDetail;
    } else if(key == InputKeyUp) cursor_up(&app->audio_menu_cursor, 4U);
    else if(key == InputKeyDown) cursor_down(&app->audio_menu_cursor, 4U);
    else if(key == InputKeyOk) {
        if(app->audio_menu_cursor == 0U) {
            app->alarm_audio_enabled[sel] = !app->alarm_audio_enabled[sel];
            alarm_save(app);
        } else if(app->audio_menu_cursor == 1U) {
            app->alarm_tone[sel] = (AlarmTone)((app->alarm_tone[sel] + 1U) % ToneCount);
            alarm_save(app);
        } else if(app->audio_menu_cursor == 2U) test_tone(app->alarm_tone[sel]);
        else {
            alarm_save(app);
            app->screen = ScreenAlarmDetail;
        }
    }
}

static void handle_counter_menu_input(Ck42xWakeupApp* app, InputKey key) {
    if(key == InputKeyBack) {
        counter_save(app);
        app->screen = ScreenSettings;
    } else if(key == InputKeyUp) cursor_up(&app->counter_menu_cursor, 4U);
    else if(key == InputKeyDown) cursor_down(&app->counter_menu_cursor, 4U);
    else if(key == InputKeyOk) {
        if(app->counter_menu_cursor == 0U) {
            app->counter_enabled = !app->counter_enabled;
            counter_save(app);
        } else if(app->counter_menu_cursor == 1U) {
            app->jumper_type = (JumperType)((app->jumper_type + 1U) % JumperCount);
            counter_save(app);
        } else if(app->counter_menu_cursor == 2U) {
            app->counter_count = 0;
            counter_save(app);
        } else {
            counter_save(app);
            app->screen = ScreenSettings;
        }
    }
}

static void handle_settings_input(Ck42xWakeupApp* app, InputKey key) {
    if(key == InputKeyBack) app->screen = ScreenMenu;
    else if(key == InputKeyUp) cursor_up(&app->settings_cursor, 5U);
    else if(key == InputKeyDown) cursor_down(&app->settings_cursor, 5U);
    else if(key == InputKeyOk) {
        if(app->settings_cursor == 0U) {
            app->seconds += 60U;
            ck42x_clock_sync_fields(app);
        } else if(app->settings_cursor == 1U) {
            app->seconds = app->seconds > 60U ? app->seconds - 60U : 0U;
            ck42x_clock_sync_fields(app);
        } else if(app->settings_cursor == 2U) {
            app->backlight_keep_on = !app->backlight_keep_on;
            backlight_apply(app);
            prefs_save(app);
        } else if(app->settings_cursor == 3U) {
            app->counter_menu_cursor = 0;
            app->screen = ScreenCounterMenu;
        } else app->screen = ScreenMenu;
    }
}

static void handle_ring_input(Ck42xWakeupApp* app, InputKey key) {
    if(key == InputKeyOk || key == InputKeyBack) {
        stop_ir_capture(app);
        stop_subghz_tx(app);
        furi_hal_vibro_on(false);
        stop_alarm_audio();
        app->screen = ScreenMain;
    }
}

static void ck42x_input_callback(InputEvent* input, void* context) {
    Ck42xWakeupApp* app = context;
    furi_message_queue_put(app->queue, input, 0);
}

int32_t ck42x_moonphase_alarm_app(void* p) {
    UNUSED(p);
    Ck42xWakeupApp* app = malloc(sizeof(Ck42xWakeupApp));
    memset(app, 0, sizeof(Ck42xWakeupApp));
    app->running = true;
    app->screen = ScreenMain;
    app->counter_enabled = true;
    app->jumper_type = JumperSheep;
    app->seconds = ck42x_initial_time_seconds();
    ck42x_clock_sync_fields(app);
    counter_load(app);
    prefs_load(app);
    app->selected_alarm = 0;
    app->ringing_alarm = 0;
    for(uint8_t i = 0; i < ALARM_COUNT; i++) {
        app->alarm_configured[i] = false;
        app->alarm_enabled[i] = false;
        app->alarm_hr[i] = 0;
        app->alarm_min[i] = 0;
        app->alarm_ir_enabled[i] = false;
        app->alarm_subg_enabled[i] = false;
        app->alarm_haptic_enabled[i] = true;
        app->alarm_audio_enabled[i] = true;
        app->alarm_tone[i] = ToneClassic;
        app->alarm_badkb_enabled[i] = false;
        app->alarm_badkb_payload[i] = BadkbOff;
        app->ir_ready[i] = false;
        app->ir_count[i] = 0;
    }
    alarm_load(app);

    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, ck42x_draw_callback, app);
    view_port_input_callback_set(app->view_port, ck42x_input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    backlight_apply(app);
    view_port_update(app->view_port);

    const uint32_t clock_tick_frequency = furi_kernel_get_tick_frequency();
    uint32_t last_clock_tick = furi_get_tick();
    while(app->running) {
        InputEvent input;
        FuriStatus status = furi_message_queue_get(app->queue, &input, LOOP_MS);
        if(status == FuriStatusOk && input.type == InputTypeShort) {
            if(app->screen == ScreenMenu) handle_menu_input(app, input.key);
            else if(app->screen == ScreenAlarmList) handle_alarm_list_input(app, input.key);
            else if(app->screen == ScreenAlarmDetail) handle_alarm_detail_input(app, input.key);
            else if(app->screen == ScreenTimeSelect) handle_time_input(app, input.key);
            else if(app->screen == ScreenIrMenu) handle_ir_menu_input(app, input.key);
            else if(app->screen == ScreenIrCapture) handle_ir_capture_input(app, input.key);
            else if(app->screen == ScreenSubgMenu) handle_subg_menu_input(app, input.key);
            else if(app->screen == ScreenBadkbMenu) handle_badkb_menu_input(app, input.key);
            else if(app->screen == ScreenAudioMenu) handle_audio_menu_input(app, input.key);
            else if(app->screen == ScreenCounterMenu) handle_counter_menu_input(app, input.key);
            else if(app->screen == ScreenSettings) handle_settings_input(app, input.key);
            else if(app->screen == ScreenAbout && input.key == InputKeyBack) app->screen = ScreenMenu;
            else if(app->screen == ScreenRing) handle_ring_input(app, input.key);
            else handle_main_input(app, input.key);
            view_port_update(app->view_port);
        }

        if(app->ir_capture_done) {
            stop_ir_capture(app);
            app->alarm_ir_enabled[app->ir_record_alarm % ALARM_COUNT] = true;
            alarm_save(app);
            view_port_update(app->view_port);
        }
        if(app->subghz_tx_active) {
            service_subghz_tx(app);
            view_port_update(app->view_port);
        }

        app->anim_tick++;
        if(app->screen == ScreenMain) view_port_update(app->view_port);

        uint32_t now_tick = furi_get_tick();
        uint32_t elapsed_ticks = now_tick - last_clock_tick;
        while(elapsed_ticks >= clock_tick_frequency) {
            last_clock_tick += clock_tick_frequency;
            elapsed_ticks -= clock_tick_frequency;
            ck42x_clock_tick(app);
            view_port_update(app->view_port);
        }
    }

    stop_ir_capture(app);
    stop_subghz_tx(app);
    furi_hal_vibro_on(false);
    stop_alarm_audio();
    counter_save(app);
    alarm_save(app);
    if(app->notification) {
        app->backlight_keep_on = false;
        backlight_apply(app);
    }
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    if(app->notification) furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->queue);
    free(app);
    return 0;
}
