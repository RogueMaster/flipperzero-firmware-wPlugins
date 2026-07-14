#include <furi.h>
#include <gui/gui.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>

// Константы для размеров сегментов
#define SEGMENT_WIDTH   16 // Ширина горизонтальных сегментов
#define SEGMENT_HEIGHT  5 // Толщина горизонтальных сегментов
#define VERTICAL_HEIGHT 20 // Высота вертикальных сегментов
#define VERTICAL_WIDTH  5 // Ширина вертикальных сегментов
#define TAPER           3 // Величина сужения на концах

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
    int mode; // 12 or 24 hour mode
} SegmentClock;

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

static void draw_callback(Canvas* canvas, void* ctx) {
    SegmentClock* clock = ctx;
    furi_mutex_acquire(clock->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

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
    if(clock->mode == 12) {
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

    furi_mutex_release(clock->mutex);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    SegmentClock* clock = ctx;
    furi_message_queue_put(clock->input_queue, input_event, FuriWaitForever);
}

static void timer_callback(void* ctx) {
    SegmentClock* clock = ctx;
    furi_mutex_acquire(clock->mutex, FuriWaitForever);

    DateTime curr_datetime;
    furi_hal_rtc_get_datetime(&curr_datetime);
    clock->datetime = curr_datetime;

    // Переключаем состояние двоеточия каждую секунду
    clock->colon_state = !clock->colon_state;

    furi_mutex_release(clock->mutex);
    view_port_update(clock->view_port);
}

int32_t seg_clock_app(void* p) {
    UNUSED(p);
    SegmentClock* clock = malloc(sizeof(SegmentClock));

    clock->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    clock->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    clock->running = true;
    clock->colon_state = true; // Инициализируем состояние двоеточия
    clock->mode = 24;

    // Setup view port
    clock->view_port = view_port_alloc();
    view_port_draw_callback_set(clock->view_port, draw_callback, clock);
    view_port_input_callback_set(clock->view_port, input_callback, clock);

    // Setup GUI and timer
    clock->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(clock->gui, clock->view_port, GuiLayerFullscreen);

    // Initialize notification system
    clock->notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(clock->notifications, &sequence_display_backlight_enforce_on);

    clock->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, clock);
    furi_timer_start(clock->timer, 500); // Обновляем каждые 500мс для мигания двоеточия

    // Get initial time
    DateTime curr_datetime;
    furi_hal_rtc_get_datetime(&curr_datetime);
    clock->datetime = curr_datetime;

    // Handle input
    InputEvent event;
    while(clock->running) {
        if(furi_message_queue_get(clock->input_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) {
                clock->running = false;
            }

            if(event.type == InputTypeShort && event.key == InputKeyUp) {
                // switch between 12 and 24 hour time
                if(clock->mode == 12) {
                    clock->mode = 24;
                } else {
                    clock->mode = 12;
                }
            }
        }
    }

    // Cleanup
    furi_timer_free(clock->timer);

    // Restore normal backlight behavior
    notification_message(clock->notifications, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);

    gui_remove_view_port(clock->gui, clock->view_port);
    view_port_free(clock->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(clock->input_queue);
    furi_mutex_free(clock->mutex);
    free(clock);

    return 0;
}
