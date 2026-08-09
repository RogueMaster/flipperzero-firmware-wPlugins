#include <furi.h>
#include <gui/gui.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>
#include <locale/locale.h>
#include <notification/notification_app.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

// Константы для размеров сегментов
#define SEGMENT_WIDTH   16 // Ширина горизонтальных сегментов
#define SEGMENT_HEIGHT  5 // Толщина горизонтальных сегментов
#define VERTICAL_HEIGHT 20 // Высота вертикальных сегментов
#define VERTICAL_WIDTH  5 // Ширина вертикальных сегментов
#define TAPER           3 // Величина сужения на концах

// Settings (alarm + brightness) persist under this fap's own data folder.
// APP_DATA_PATH is the "/data" alias -> /ext/apps_data/segment_clock/.
#define SEG_SETTINGS_PATH    APP_DATA_PATH("settings.save")
#define SEG_SETTINGS_DIR     STORAGE_APP_DATA_PATH_PREFIX // "/data", made at startup
#define SEG_SETTINGS_MAGIC   0x53 // 'S'
#define SEG_SETTINGS_VERSION 3 // v3: added brightness

// How long the brightness bar stays on screen after Up/Down.
#define BRIGHTNESS_OSD_MS 3000

typedef enum {
    ScreenClock,
    ScreenAlarmMenu,
    ScreenAlarmTime,
} Screen;

typedef struct {
    bool alarm_enabled;
    uint8_t alarm_hour;
    uint8_t alarm_minute;
    uint8_t brightness; // 0..100, restored and applied on next launch
} SegSettings;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notifications;
    FuriTimer* timer;
    DateTime datetime;
    bool running;
    bool colon_state;
    LocaleTimeFormat time_format; // 12h/24h from the system locale (read once)

    SegSettings settings; // persisted (alarm)

    // Brightness runtime (like the Nightstand clock)
    int brightness; // 0..100
    bool led; // red nightlight when brightness is 0
    uint32_t brightness_shown_until; // brightness bar visible until this tick

    // Alarm runtime
    bool alarm_firing; // currently ringing
    bool alarm_flash_on; // flash toggle while firing
    bool alarm_consumed; // already fired for the current matching minute
    uint8_t melody_phase; // replay the melody every other tick

    // Setup UI
    Screen screen;
    uint8_t menu_index; // 0 = enable toggle, 1 = set time
    uint8_t edit_hour; // scratch for the time picker
    uint8_t edit_minute;
    uint8_t edit_field; // 0 = hour, 1 = minute, 2 = meridian (12h only)
} SegmentClock;

// Custom alarm melody: an urgent high/low two-tone siren with vibro and the red
// LED, distinct from the Nightstand clock's rising chirp. Short enough to replay
// roughly once a second while the alarm rings.
static const NotificationSequence alarm_melody = {
    &message_vibro_on,
    &message_red_255,
    &message_note_d6,
    &message_delay_100,
    &message_note_a5,
    &message_delay_100,
    &message_note_d6,
    &message_delay_100,
    &message_note_a5,
    &message_delay_100,
    &message_note_d6,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

static const NotificationSequence led_off_seq = {
    &message_red_0,
    NULL,
};

// Dim red nightlight, held on (do_not_reset) once brightness reaches 0.
static const NotificationMessage message_red_dim = {
    .type = NotificationMessageTypeLedRed,
    .data.led.value = 0xFF / 16,
};
static const NotificationSequence led_on = {
    &message_red_dim,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence led_off = {
    &message_red_0,
    &message_do_not_reset,
    NULL,
};

// Push a brightness level to the panel even while the backlight is held on. A
// plain backlight_on early-returns when the backlight is already on, so force_on
// is used to update the panel now and the enforce pair re-locks the level.
static void set_enforced_brightness(NotificationApp* notif, float value) {
    notif->settings.display_brightness = value;
    notification_message(notif, &sequence_display_backlight_enforce_on);
    notification_message(notif, &sequence_display_backlight_enforce_auto);
    notification_message(notif, &sequence_display_backlight_enforce_on);
}

static void set_backlight_brightness(NotificationApp* notif, float value) {
    notif->settings.display_brightness = value;
    notification_message(notif, &sequence_display_backlight_enforce_on);
}

static void show_brightness_osd(SegmentClock* clock) {
    clock->brightness_shown_until = furi_get_tick() + furi_ms_to_ticks(BRIGHTNESS_OSD_MS);
}

static void handle_up(SegmentClock* clock) {
    show_brightness_osd(clock);
    if(clock->brightness < 100) {
        clock->led = false;
        notification_message(clock->notifications, &led_off);
        clock->brightness += 5;
        if(clock->brightness > 100) clock->brightness = 100;
    }
    set_enforced_brightness(clock->notifications, (float)(clock->brightness / 100.f));
}

static void handle_down(SegmentClock* clock) {
    show_brightness_osd(clock);
    if(clock->brightness > 0) {
        clock->brightness -= 5;
        if(clock->brightness < 0) clock->brightness = 0;
        if(clock->brightness == 0) { // first 5 -> 0 transition turns the nightlight on
            clock->led = true;
            notification_message(clock->notifications, &led_on);
        }
    } else { // already 0: each Down toggles the nightlight
        clock->led = !clock->led;
        notification_message(clock->notifications, clock->led ? &led_on : &led_off);
    }
    set_enforced_brightness(clock->notifications, (float)(clock->brightness / 100.f));
}

/*
 7-segment display layout:
    AAAA
   F    B
   F    B
    GGGG
   E    C
   E    C
    DDDD

   Bit pattern:
   7 6 5 4 3 2 1 0
   A B C D E F G -
*/

// 7-segment display patterns for digits 0-9
const uint8_t SEGMENT_PATTERNS[] = {
    0b11111100, // 0: ABCDEF
    0b01100000, // 1: BC
    0b11011010, // 2: ABDEG
    0b11110010, // 3: ABCDG
    0b01100110, // 4: BCFG
    0b10110110, // 5: ACDFG
    0b10111110, // 6: ACDEFG
    0b11100000, // 7: ABC
    0b11111110, // 8: ABCDEFG
    0b11110110 // 9: ABCDFG
};

// Draw a single 7-segment digit
void draw_digit(Canvas* canvas, uint8_t x, uint8_t y, uint8_t digit) {
    uint8_t pattern = SEGMENT_PATTERNS[digit];
    uint8_t segment_width = SEGMENT_WIDTH;
    uint8_t segment_height = SEGMENT_HEIGHT;
    uint8_t vertical_height = VERTICAL_HEIGHT;
    uint8_t vertical_width = VERTICAL_WIDTH;
    uint8_t taper = TAPER;

    // Горизонтальные сегменты (A, G, D) с сужением
    if(pattern & 0b10000000) { // A (верх)
        for(int i = 0; i < segment_height; i++) {
            uint8_t taper_amount = (taper * i) / (segment_height - 1);
            canvas_draw_line(
                canvas,
                x + vertical_width + taper_amount,
                y + i,
                x + vertical_width + segment_width - taper_amount,
                y + i);
        }
    }

    if(pattern & 0b00000010) { // G (середина)
        uint8_t y_mid = y + vertical_height;
        for(int i = 0; i < segment_height; i++) {
            float center_ratio =
                fabsf((float)i - (segment_height - 1) / 2.0f) / ((segment_height - 1) / 2.0f);
            uint8_t taper_amount = (uint8_t)(taper * center_ratio);

            canvas_draw_line(
                canvas,
                x + vertical_width + taper_amount,
                y_mid + i,
                x + vertical_width + segment_width - taper_amount,
                y_mid + i);
        }
    }

    if(pattern & 0b00010000) { // D (низ)
        uint8_t y_bottom = y + vertical_height * 2;
        for(int i = 0; i < segment_height; i++) {
            uint8_t taper_amount = (taper * (segment_height - i - 1)) / (segment_height - 1);
            canvas_draw_line(
                canvas,
                x + vertical_width + taper_amount,
                y_bottom + i,
                x + vertical_width + segment_width - taper_amount,
                y_bottom + i);
        }
    }

    // Вертикальные сегменты (F, B, E, C)
    if(pattern & 0b00000100) { // F (лево верх)
        for(int i = 0; i < vertical_height - segment_height; i++) {
            uint8_t taper_amount = 0;
            if(i < segment_height) // Сужение сверху
                taper_amount = (taper * (segment_height - i - 1)) / segment_height;
            else if(i > vertical_height - segment_height * 2) // Сужение снизу
                taper_amount =
                    (taper * (i - (vertical_height - segment_height * 2))) / segment_height;

            for(int w = 0; w < vertical_width - taper_amount; w++) {
                canvas_draw_dot(canvas, x + w, y + segment_height + i);
            }
        }
    }

    if(pattern & 0b01000000) { // B (право верх)
        for(int i = 0; i < vertical_height - segment_height; i++) {
            uint8_t taper_amount = 0;
            if(i < segment_height) // Сужение сверху
                taper_amount = (taper * (segment_height - i - 1)) / segment_height;
            else if(i > vertical_height - segment_height * 2) // Сужение снизу
                taper_amount =
                    (taper * (i - (vertical_height - segment_height * 2))) / segment_height;

            for(int w = 0; w < vertical_width - taper_amount; w++) {
                canvas_draw_dot(
                    canvas,
                    x + vertical_width + segment_width + vertical_width - w - 1,
                    y + segment_height + i);
            }
        }
    }

    if(pattern & 0b00001000) { // E (лево низ)
        for(int i = 0; i < vertical_height - segment_height; i++) {
            uint8_t taper_amount = 0;
            if(i < segment_height) // Сужение сверху
                taper_amount = (taper * (segment_height - i - 1)) / segment_height;
            else if(i > vertical_height - segment_height * 2) // Сужение снизу
                taper_amount =
                    (taper * (i - (vertical_height - segment_height * 2))) / segment_height;

            for(int w = 0; w < vertical_width - taper_amount; w++) {
                canvas_draw_dot(canvas, x + w, y + vertical_height + segment_height + i);
            }
        }
    }

    if(pattern & 0b00100000) { // C (право низ)
        for(int i = 0; i < vertical_height - segment_height; i++) {
            uint8_t taper_amount = 0;
            if(i < segment_height) // Сужение сверху
                taper_amount = (taper * (segment_height - i - 1)) / segment_height;
            else if(i > vertical_height - segment_height * 2) // Сужение снизу
                taper_amount =
                    (taper * (i - (vertical_height - segment_height * 2))) / segment_height;

            for(int w = 0; w < vertical_width - taper_amount; w++) {
                canvas_draw_dot(
                    canvas,
                    x + vertical_width + segment_width + vertical_width - w - 1,
                    y + vertical_height + segment_height + i);
            }
        }
    }
}

// ---- alarm helpers (24h stored, 12h display when mode == 12) ----

static void to_12h(uint8_t h24, uint8_t* h12, bool* pm) {
    *pm = h24 >= 12;
    uint8_t h = h24 % 12;
    *h12 = (h == 0) ? 12 : h;
}

static uint8_t to_24h(uint8_t h12, bool pm) {
    return (h12 % 12) + (pm ? 12 : 0);
}

static void format_alarm_time(char* buf, size_t n, uint8_t h24, uint8_t minute, bool h12mode) {
    if(h12mode) {
        uint8_t h12;
        bool pm;
        to_12h(h24, &h12, &pm);
        snprintf(buf, n, "%u:%.2u %s", h12, minute, pm ? "PM" : "AM");
    } else {
        snprintf(buf, n, "%.2u:%.2u", h24, minute);
    }
}

static void seg_settings_save(SegmentClock* clock) {
    clock->settings.brightness = (uint8_t)clock->brightness; // keep the saved level current
    saved_struct_save(
        SEG_SETTINGS_PATH,
        &clock->settings,
        sizeof(SegSettings),
        SEG_SETTINGS_MAGIC,
        SEG_SETTINGS_VERSION);
}

// ---- draw: called with the mutex held ----

static void draw_clock_face(Canvas* canvas, SegmentClock* clock) {
    uint8_t hours = clock->datetime.hour;
    uint8_t minutes = clock->datetime.minute;

    // Центрируем дисплей на экране
    uint8_t digit_width = SEGMENT_WIDTH + VERTICAL_WIDTH * 2; // Полная ширина одной цифры
    uint8_t digit_spacing = 4; // Расстояние между цифрами
    uint8_t colon_width = 4; // Ширина точек
    uint8_t colon_spacing = 2; // Уменьшаем расстояние до/после двоеточия

    // Общая ширина: 4 цифры + промежутки между цифрами + двоеточие с отступами
    uint8_t total_width =
        (digit_width * 4) + (digit_spacing * 3) + colon_width + (colon_spacing * 2);
    uint8_t screen_width = 128; // Ширина экрана Flipper Zero
    uint8_t screen_height = 64; // Высота экрана Flipper Zero
    uint8_t start_x = (screen_width - total_width) / 2; // Центрируем по горизонтали
    uint8_t start_y = (screen_height - VERTICAL_HEIGHT * 2 - SEGMENT_HEIGHT) / 2;

    // Draw hours
    if(clock->time_format == LocaleTimeFormat12h) {
        // Convert to 12-hour format
        uint8_t display_hours = hours % 12;
        if(display_hours == 0) display_hours = 12; // Handle midnight/noon case

        draw_digit(canvas, start_x, start_y, display_hours / 10);
        draw_digit(canvas, start_x + digit_width + digit_spacing, start_y, display_hours % 10);
    } else {
        // no modification for 24 hour mode
        draw_digit(canvas, start_x, start_y, hours / 10);
        draw_digit(canvas, start_x + digit_width + digit_spacing, start_y, hours % 10);
    }

    // Draw colon (только если colon_state == true)
    if(clock->colon_state) {
        // Смещаем двоеточие немного влево
        uint8_t colon_x = start_x + (digit_width * 2) + (digit_spacing * 2) + colon_spacing - 1;
        uint8_t colon_y_top = start_y + VERTICAL_HEIGHT - VERTICAL_HEIGHT / 2;
        uint8_t colon_y_bottom = start_y + VERTICAL_HEIGHT + VERTICAL_HEIGHT / 2;
        canvas_draw_box(canvas, colon_x, colon_y_top, colon_width, colon_width);
        canvas_draw_box(canvas, colon_x, colon_y_bottom, colon_width, colon_width);
    }

    // Draw minutes
    uint8_t minutes_x = start_x + (digit_width * 2) + (digit_spacing * 2) + colon_spacing +
                        colon_width + colon_spacing;
    draw_digit(canvas, minutes_x, start_y, minutes / 10);
    draw_digit(canvas, minutes_x + digit_width + digit_spacing, start_y, minutes % 10);

    // AM/PM in the lower-right corner when the locale uses 12-hour time.
    if(clock->time_format == LocaleTimeFormat12h) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 126, 62, AlignRight, AlignBottom, (hours >= 12) ? "PM" : "AM");
    }

    // A small filled dot in the top-left corner marks the alarm as armed - kept
    // minimal so it never crowds the segment digits.
    if(clock->settings.alarm_enabled) {
        canvas_draw_disc(canvas, 5, 5, 3);
    }

    // Brightness bar along the bottom for 3s after Up/Down (a right-edge bar
    // would sit on top of the wide segment digits, so it goes here instead).
    // Drawn thin by hand: elements_progress_bar is ~9px tall and would clip off
    // the bottom of the 64px screen here.
    if(furi_get_tick() < clock->brightness_shown_until) {
        const uint8_t bx = 14, by = 59, bw = 100, bh = 4;
        canvas_draw_frame(canvas, bx, by, bw, bh);
        uint8_t fill = (uint8_t)((bw - 2) * clock->brightness / 100);
        if(fill) canvas_draw_box(canvas, bx + 1, by + 1, fill, bh - 2);
    }
}

static void draw_alarm_firing(Canvas* canvas, SegmentClock* clock) {
    if(clock->alarm_flash_on) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    // FontBigNumbers has no letters, so ALARM has to use a text font.
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "! ALARM !");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "Press any key to stop");
}

static void draw_alarm_menu(Canvas* canvas, SegmentClock* clock) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 3, AlignCenter, AlignTop, "Alarm");

    char timebuf[12];
    format_alarm_time(
        timebuf,
        sizeof(timebuf),
        clock->settings.alarm_hour,
        clock->settings.alarm_minute,
        clock->time_format == LocaleTimeFormat12h);
    char row0[24], row1[24];
    snprintf(row0, sizeof(row0), "Alarm:  %s", clock->settings.alarm_enabled ? "ON" : "OFF");
    snprintf(row1, sizeof(row1), "Time:   %s", timebuf);
    const char* rows[2] = {row0, row1};

    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < 2; i++) {
        uint8_t y = 22 + i * 15;
        if(clock->menu_index == i) {
            canvas_draw_box(canvas, 4, y - 2, 120, 14);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(canvas, 10, y + 5, AlignLeft, AlignCenter, rows[i]);
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "OK select   Back exit");
}

static void draw_alarm_time(Canvas* canvas, SegmentClock* clock) {
    bool h12mode = (clock->time_format == LocaleTimeFormat12h);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Alarm time");

    char hh[4], mm[4];
    bool pm = false;
    if(h12mode) {
        uint8_t h12;
        to_12h(clock->edit_hour, &h12, &pm);
        snprintf(hh, sizeof(hh), "%.2u", h12);
    } else {
        snprintf(hh, sizeof(hh), "%.2u", clock->edit_hour);
    }
    snprintf(mm, sizeof(mm), "%.2u", clock->edit_minute);

    uint8_t hx = h12mode ? 34 : 45;
    uint8_t cx = h12mode ? 53 : 64;
    uint8_t mx = h12mode ? 72 : 83;

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, hx, 32, AlignCenter, AlignCenter, hh);
    canvas_draw_str_aligned(canvas, cx, 32, AlignCenter, AlignCenter, ":");
    canvas_draw_str_aligned(canvas, mx, 32, AlignCenter, AlignCenter, mm);
    if(h12mode) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 104, 32, AlignCenter, AlignCenter, pm ? "PM" : "AM");
    }

    uint8_t ux, uw;
    if(clock->edit_field == 0) {
        ux = hx;
        uw = 20;
    } else if(clock->edit_field == 1) {
        ux = mx;
        uw = 20;
    } else {
        ux = 104;
        uw = 18;
    }
    canvas_draw_line(canvas, ux - uw / 2, 46, ux + uw / 2, 46);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "Up/Down set   OK save");
}

static void draw_callback(Canvas* canvas, void* ctx) {
    SegmentClock* clock = ctx;
    furi_mutex_acquire(clock->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(clock->alarm_firing) {
        draw_alarm_firing(canvas, clock);
    } else {
        switch(clock->screen) {
        case ScreenAlarmMenu:
            draw_alarm_menu(canvas, clock);
            break;
        case ScreenAlarmTime:
            draw_alarm_time(canvas, clock);
            break;
        case ScreenClock:
        default:
            draw_clock_face(canvas, clock);
            break;
        }
    }

    furi_mutex_release(clock->mutex);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    SegmentClock* clock = ctx;
    furi_message_queue_put(clock->input_queue, input_event, FuriWaitForever);
}

static void seg_check_alarm(SegmentClock* clock) {
    if(!clock->settings.alarm_enabled) {
        clock->alarm_consumed = false;
        return;
    }
    bool match = (clock->datetime.hour == clock->settings.alarm_hour) &&
                 (clock->datetime.minute == clock->settings.alarm_minute);
    if(match) {
        if(!clock->alarm_consumed && !clock->alarm_firing) {
            clock->alarm_firing = true;
            clock->alarm_consumed = true; // don't re-arm until the minute passes
            clock->alarm_flash_on = true;
            clock->melody_phase = 0;
        }
    } else {
        clock->alarm_consumed = false;
    }
}

static void timer_callback(void* ctx) {
    SegmentClock* clock = ctx;
    furi_mutex_acquire(clock->mutex, FuriWaitForever);

    furi_hal_rtc_get_datetime(&clock->datetime);
    clock->colon_state = !clock->colon_state;

    seg_check_alarm(clock);
    if(clock->alarm_firing) {
        clock->alarm_flash_on = !clock->alarm_flash_on;
        // Timer is 500ms; replay the ~1s melody every other tick.
        // notification_message just enqueues, so it's safe from the timer task.
        if(clock->melody_phase == 0) notification_message(clock->notifications, &alarm_melody);
        clock->melody_phase ^= 1;
    }

    furi_mutex_release(clock->mutex);
    view_port_update(clock->view_port);
}

// ---- per-screen input, called with the mutex held ----

static void input_alarm_menu(SegmentClock* clock, InputEvent* event) {
    switch(event->key) {
    case InputKeyUp:
    case InputKeyDown:
        clock->menu_index = clock->menu_index ? 0 : 1;
        break;
    case InputKeyLeft:
    case InputKeyRight:
        if(clock->menu_index == 0) {
            clock->settings.alarm_enabled = !clock->settings.alarm_enabled;
            seg_settings_save(clock);
        }
        break;
    case InputKeyOk:
        if(clock->menu_index == 0) {
            clock->settings.alarm_enabled = !clock->settings.alarm_enabled;
            seg_settings_save(clock);
        } else {
            clock->edit_hour = clock->settings.alarm_hour;
            clock->edit_minute = clock->settings.alarm_minute;
            clock->edit_field = 0;
            clock->screen = ScreenAlarmTime;
        }
        break;
    case InputKeyBack:
        clock->screen = ScreenClock;
        break;
    default:
        break;
    }
}

static void input_alarm_time(SegmentClock* clock, InputEvent* event) {
    bool h12mode = (clock->time_format == LocaleTimeFormat12h);
    uint8_t max_field = h12mode ? 2 : 1;

    switch(event->key) {
    case InputKeyUp:
    case InputKeyDown: {
        bool up = (event->key == InputKeyUp);
        if(clock->edit_field == 0) {
            if(h12mode) {
                uint8_t h12;
                bool pm;
                to_12h(clock->edit_hour, &h12, &pm);
                h12 = up ? ((h12 % 12) + 1) : ((h12 == 1) ? 12 : h12 - 1);
                clock->edit_hour = to_24h(h12, pm);
            } else {
                clock->edit_hour = (clock->edit_hour + (up ? 1 : 23)) % 24;
            }
        } else if(clock->edit_field == 1) {
            clock->edit_minute = (clock->edit_minute + (up ? 1 : 59)) % 60;
        } else { // meridian: either key flips AM/PM
            uint8_t h12;
            bool pm;
            to_12h(clock->edit_hour, &h12, &pm);
            clock->edit_hour = to_24h(h12, !pm);
        }
        break;
    }
    case InputKeyLeft:
        if(clock->edit_field > 0) clock->edit_field--;
        break;
    case InputKeyRight:
        if(clock->edit_field < max_field) clock->edit_field++;
        break;
    case InputKeyOk:
    case InputKeyBack:
        // Both confirm - leaving always keeps the value shown.
        clock->settings.alarm_hour = clock->edit_hour;
        clock->settings.alarm_minute = clock->edit_minute;
        seg_settings_save(clock);
        clock->screen = ScreenAlarmMenu;
        break;
    default:
        break;
    }
}

static void input_clock(SegmentClock* clock, InputEvent* event) {
    // 12h/24h now follows the system locale, so Up/Down drive brightness (like
    // the Nightstand clock): Down at 0 toggles the red nightlight.
    switch(event->key) {
    case InputKeyBack:
        clock->running = false;
        break;
    case InputKeyUp:
        handle_up(clock);
        break;
    case InputKeyDown:
        handle_down(clock);
        break;
    case InputKeyRight:
        clock->menu_index = 0;
        clock->screen = ScreenAlarmMenu;
        break;
    default:
        break;
    }
}

int32_t seg_clock_app(void* p) {
    UNUSED(p);
    SegmentClock* clock = malloc(sizeof(SegmentClock));
    memset(clock, 0, sizeof(SegmentClock));

    clock->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    clock->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    clock->running = true;
    clock->colon_state = true;
    clock->screen = ScreenClock;

    // 12h/24h comes from the system locale, read once here.
    clock->time_format = locale_get_time_format();

    // Open notifications first so we can read (and later restore) the system
    // brightness and backlight delay.
    clock->notifications = furi_record_open(RECORD_NOTIFICATION);
    float SavedBrightness = clock->notifications->settings.display_brightness;
    uint32_t Saved_display_off_delay_ms = clock->notifications->settings.display_off_delay_ms;
    clock->notifications->settings.display_off_delay_ms = 0;

    // Make sure the data folder exists, then load settings (alarm + brightness).
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, SEG_SETTINGS_DIR);
    furi_record_close(RECORD_STORAGE);
    if(saved_struct_load(
           SEG_SETTINGS_PATH,
           &clock->settings,
           sizeof(SegSettings),
           SEG_SETTINGS_MAGIC,
           SEG_SETTINGS_VERSION)) {
        clock->brightness = clock->settings.brightness;
        if(clock->brightness > 100) clock->brightness = 100;
    } else {
        clock->settings.alarm_enabled = false;
        clock->settings.alarm_hour = 7;
        clock->settings.alarm_minute = 0;
        clock->brightness = (int)roundf(SavedBrightness * 100.f);
        clock->settings.brightness = (uint8_t)clock->brightness;
    }

    // Setup view port
    clock->view_port = view_port_alloc();
    view_port_draw_callback_set(clock->view_port, draw_callback, clock);
    view_port_input_callback_set(clock->view_port, input_callback, clock);

    // Setup GUI and timer
    clock->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(clock->gui, clock->view_port, GuiLayerFullscreen);

    // Hold the backlight on and apply the restored brightness.
    notification_message(clock->notifications, &sequence_display_backlight_enforce_on);
    set_enforced_brightness(clock->notifications, (float)(clock->brightness / 100.f));

    clock->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, clock);
    furi_timer_start(clock->timer, 500); // Обновляем каждые 500мс для мигания двоеточия

    // Get initial time
    furi_hal_rtc_get_datetime(&clock->datetime);

    // Handle input
    InputEvent event;
    while(clock->running) {
        if(furi_message_queue_get(clock->input_queue, &event, 100) == FuriStatusOk) {
            furi_mutex_acquire(clock->mutex, FuriWaitForever);

            // Any key stops a ringing alarm and nothing else. Dismiss on the
            // release click (Short) or a long press, never on Press: a tap is
            // Press -> Release -> Short, and clearing the flag on Press would let
            // the following Short fall through to the normal handler.
            if(clock->alarm_firing) {
                if(event.type == InputTypeShort || event.type == InputTypeLong) {
                    clock->alarm_firing = false;
                    notification_message(clock->notifications, &led_off_seq);
                }
            } else if(event.type == InputTypeShort) {
                switch(clock->screen) {
                case ScreenAlarmMenu:
                    input_alarm_menu(clock, &event);
                    break;
                case ScreenAlarmTime:
                    input_alarm_time(clock, &event);
                    break;
                case ScreenClock:
                default:
                    input_clock(clock, &event);
                    break;
                }
            } else if(event.type == InputTypeRepeat && clock->screen == ScreenAlarmTime) {
                // Let held Up/Down run through the time values.
                if(event.key == InputKeyUp || event.key == InputKeyDown)
                    input_alarm_time(clock, &event);
            }

            furi_mutex_release(clock->mutex);
            view_port_update(clock->view_port);
        }
    }

    // Persist final changes (captures the current brightness too).
    seg_settings_save(clock);

    // Cleanup
    furi_timer_free(clock->timer);

    // Restore the display backlight settings we changed on entry.
    clock->notifications->settings.display_off_delay_ms = Saved_display_off_delay_ms;
    notification_message(clock->notifications, &sequence_display_backlight_enforce_auto);
    set_backlight_brightness(clock->notifications, SavedBrightness);
    notification_message(clock->notifications, &led_off_seq);
    furi_record_close(RECORD_NOTIFICATION);

    gui_remove_view_port(clock->gui, clock->view_port);
    view_port_free(clock->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(clock->input_queue);
    furi_mutex_free(clock->mutex);
    free(clock);

    return 0;
}
