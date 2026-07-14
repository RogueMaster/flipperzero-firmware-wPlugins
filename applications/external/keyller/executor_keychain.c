#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/canvas.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <furi_hal_speaker.h>
#include <furi_hal_light.h>
#include <math.h>

#define MAX_BUTTONS    8
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define BUTTON_WIDTH   8
#define BUTTON_HEIGHT  12
#define BUTTON_SPACING 3
#define SOUND_DURATION 500

typedef struct {
    Gui* gui;
    NotificationApp* notifications;
    uint8_t selected_button;
    bool is_playing;
    bool running;
    FuriThread* sound_thread;
    bool button_pressed;
    uint32_t current_sound;
    FuriMutex* mutex;
    uint8_t led_color;
    uint8_t circle_size;
    bool circle_growing;
    const char* current_name;
} ExecutorKeychainApp;

typedef struct {
    uint8_t x;
    uint8_t y;
} ButtonPosition;

static const ButtonPosition BUTTON_POSITIONS[MAX_BUTTONS] = {
    {8, 70}, // Botón 1
    {21, 70}, // Botón 2
    {34, 70}, // Botón 3
    {47, 70}, // Botón 4
    {8, 84}, // Botón 5
    {21, 84}, // Botón 6
    {34, 84}, // Botón 7
    {47, 84} // Botón 8
};

static const char* NAMES[] = {"EXECUTOR", "Echo Keyller", "Flpr Keyller"};
static const int NUM_NAMES = 3;

static void executor_keychain_draw_callback(Canvas* canvas, void* ctx) {
    ExecutorKeychainApp* app = ctx;
    canvas_clear(canvas);

    canvas_draw_rframe(canvas, 0, 0, 64, 128, 8);
    canvas_invert_color(canvas);
    canvas_draw_box(canvas, 0, 0, 64, 10);
    canvas_invert_color(canvas);
    canvas_draw_circle(canvas, 62, 0, 10);
    canvas_draw_circle(canvas, -1, 0, 10);
    canvas_draw_line(canvas, 10, 0, 52, 0);

    canvas_draw_circle(canvas, 31, 35, 24);
    canvas_draw_circle(canvas, 31, 35, 25);

    canvas_draw_line(canvas, 8, 32, 34, 58);
    canvas_draw_line(canvas, 10, 27, 39, 56);

    canvas_draw_line(canvas, 7, 38, 27, 58);

    canvas_draw_line(canvas, 12, 22, 44, 54);

    canvas_draw_line(canvas, 15, 17, 48, 51);

    canvas_draw_line(canvas, 20, 14, 52, 47);

    canvas_draw_line(canvas, 26, 13, 53, 40);

    canvas_draw_line(canvas, 33, 12, 55, 34);
    canvas_draw_rframe(canvas, 3, 67, 58, 32, 5);

    canvas_draw_line(canvas, 3, 14, 3, 64);
    canvas_draw_line(canvas, 60, 14, 60, 64);
    canvas_draw_line(canvas, 13, 3, 48, 3);
    canvas_draw_line(canvas, 7, 124, 58, 124);

    for(int i = 0; i < MAX_BUTTONS; i++) {
        if(i == app->selected_button) {
            canvas_draw_rframe(
                canvas,
                BUTTON_POSITIONS[i].x - 1,
                BUTTON_POSITIONS[i].y - 1,
                BUTTON_WIDTH + 2,
                BUTTON_HEIGHT + 2,
                2);
        }
        // Dibuja el botón normal
        canvas_draw_rframe(
            canvas, BUTTON_POSITIONS[i].x, BUTTON_POSITIONS[i].y, BUTTON_WIDTH, BUTTON_HEIGHT, 2);
    }

    canvas_draw_line(canvas, 1, 104, 63, 104);
    canvas_draw_line(canvas, 1, 108, 1, 109);
    canvas_draw_line(canvas, 1, 114, 1, 113);
    canvas_draw_line(canvas, 1, 119, 1, 118);
    canvas_draw_line(canvas, 62, 108, 62, 109);
    canvas_draw_line(canvas, 62, 114, 62, 113);
    canvas_draw_line(canvas, 62, 119, 62, 118);

    canvas_draw_disc(canvas, 31, 35, app->circle_size);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 118, app->current_name);
}

static void executor_keychain_play_continuous_sound(ExecutorKeychainApp* app) {
    if(!app->button_pressed || !app->running) return;

    switch(app->current_sound) {
    case 0: // Sirena
        for(size_t i = 0; i <= 3; i++) {
            if(!app->button_pressed) break;
            furi_hal_speaker_start(2217, 1.0f);
            furi_delay_ms(97);
            for(int i = 2150; i >= 2050; i -= 50) {
                furi_hal_speaker_start(i, 1.0f);
                furi_delay_ms(110);
            }
        }
        break;
    case 1: // Alarma de proximidad
        for(int i = 0; i < 6; i++) {
            if(!app->button_pressed) break;
            furi_hal_speaker_start(800, 1.0f);
            furi_delay_ms(60);

            if(!app->button_pressed) break;
            furi_delay_ms(20);
            furi_hal_speaker_start(600, 1.0f);
            furi_delay_ms(60);

            if(!app->button_pressed) break;
            furi_delay_ms(20);
        }
        break;
    case 2: // Bomba
        for(int i = 2300; i >= 580; i -= 50) {
            if(!app->button_pressed) break;
            furi_hal_speaker_start(i, 1.0f);
            furi_delay_ms(40);
        }

        // Explosión
        for(int j = 180; j > 0; j -= 10) {
            furi_hal_speaker_start(j, 1.0f);
            if(!app->button_pressed) break;
            furi_delay_ms(20);
            furi_hal_speaker_start(j + 10, 1.0f);
            if(!app->button_pressed) break;
            furi_delay_ms(20);
            furi_hal_speaker_start(j - 10, 1.0f);
            if(!app->button_pressed) break;
            furi_delay_ms(20);
            furi_hal_speaker_start(j + 10, 1.0f);
            if(!app->button_pressed) break;
            furi_delay_ms(20);
        }
        break;
    case 3: // Pew pew
        for(int i = 0; i < 3; i++) {
            if(!app->button_pressed) break;
            furi_hal_speaker_start(180, 1.0f);
            if(!app->button_pressed) break;
            furi_delay_ms(20);
            furi_hal_speaker_start(180 + 10, 1.0f);
            if(!app->button_pressed) break;
            furi_delay_ms(20);
            furi_hal_speaker_start(100 - 10, 1.0f);
            if(!app->button_pressed) break;
            furi_delay_ms(20);
            furi_hal_speaker_start(80 + 10, 1.0f);
            if(!app->button_pressed) break;

            for(int freq = 2200; freq >= 800; freq -= 100) {
                if(!app->button_pressed) break;
                furi_hal_speaker_start(freq, 1.0f);
                if(!app->button_pressed) break;
                furi_delay_ms(20);
            }
        }
        break;
    case 4: // Chinese toy 1
        for(int i = 0; i < 4; i++) {
            if(!app->button_pressed) break;
            furi_hal_speaker_start(1000, 1.0f);
            if(!app->button_pressed) break;
            furi_delay_ms(200);
            furi_hal_speaker_start(800, 1.0f);
            if(!app->button_pressed) break;
            furi_delay_ms(200);
        }
        break;
    case 5: // Phone
        for(int i = 0; i < 6; i++) {
            if(!app->button_pressed) break;
            furi_hal_speaker_start(800, 1.0f);
            furi_delay_ms(30);
            if(!app->button_pressed) break;
            furi_hal_speaker_start(600, 1.0f);
            furi_delay_ms(30);
        }
        break;
    case 6: // Chinese toy 2
        for(int i = 0; i < 10; i++) {
            if(!app->button_pressed) break;
            for(int phase = 800; phase <= 1000; phase += 60) {
                if(!app->button_pressed) break;
                int freq = phase;
                furi_hal_speaker_start(freq, 1.0f);
                if(!app->button_pressed) break;
                furi_delay_ms(15);
            }
            for(int phase = 1000; phase > 800; phase -= 60) {
                if(!app->button_pressed) break;
                int freq = phase;
                furi_hal_speaker_start(freq, 1.0f);
                if(!app->button_pressed) break;
                furi_delay_ms(15);
            }
        }
        break;
    case 7:
        // Machine gun
        furi_hal_speaker_start(200, 1.0f);
        furi_delay_ms(10);
        if(!app->button_pressed) break;
        furi_hal_speaker_start(250, 1.0f);
        furi_delay_ms(10);
        if(!app->button_pressed) break;
        furi_hal_speaker_start(100, 1.0f);
        furi_delay_ms(10);
        if(!app->button_pressed) break;
        furi_hal_speaker_start(50, 1.0f);
        furi_delay_ms(10);
        break;
    }
}

static int32_t executor_keychain_sound_thread_callback(void* context) {
    ExecutorKeychainApp* app = context;
    bool speaker_acquired = false;

    while(app->running) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool should_play = app->button_pressed && !app->is_playing;
        furi_mutex_release(app->mutex);

        if(should_play) {
            if(furi_hal_speaker_acquire(1000)) {
                speaker_acquired = true;
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->is_playing = true;
                app->current_sound = app->selected_button;
                app->circle_growing = true;
                app->circle_size = 8;
                furi_mutex_release(app->mutex);

                while(true) {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    bool continue_playing = app->button_pressed && app->running;

                    // Actualizar el tamaño del círculo con efecto de pulso suave
                    if(app->circle_growing) {
                        app->circle_size++;
                        if(app->circle_size >= 16) app->circle_growing = false;
                    } else {
                        app->circle_size--;
                        if(app->circle_size <= 8) app->circle_growing = true;
                    }

                    // Actualizar el color del LED con efecto arcoíris
                    app->led_color = (app->led_color + 1) % 12;
                    switch(app->led_color) {
                    case 0:
                        furi_hal_light_set(LightRed, 0xff);
                        furi_hal_light_set(LightGreen, 0x00);
                        furi_hal_light_set(LightBlue, 0x00);
                        break;
                    case 1:
                        furi_hal_light_set(LightRed, 0xff);
                        furi_hal_light_set(LightGreen, 0x40);
                        furi_hal_light_set(LightBlue, 0x00);
                        break;
                    case 2:
                        furi_hal_light_set(LightRed, 0xff);
                        furi_hal_light_set(LightGreen, 0xff);
                        furi_hal_light_set(LightBlue, 0x00);
                        break;
                    case 3:
                        furi_hal_light_set(LightRed, 0x40);
                        furi_hal_light_set(LightGreen, 0xff);
                        furi_hal_light_set(LightBlue, 0x00);
                        break;
                    case 4:
                        furi_hal_light_set(LightRed, 0x00);
                        furi_hal_light_set(LightGreen, 0xff);
                        furi_hal_light_set(LightBlue, 0x00);
                        break;
                    case 5:
                        furi_hal_light_set(LightRed, 0x00);
                        furi_hal_light_set(LightGreen, 0xff);
                        furi_hal_light_set(LightBlue, 0x40);
                        break;
                    case 6:
                        furi_hal_light_set(LightRed, 0x00);
                        furi_hal_light_set(LightGreen, 0xff);
                        furi_hal_light_set(LightBlue, 0xff);
                        break;
                    case 7:
                        furi_hal_light_set(LightRed, 0x00);
                        furi_hal_light_set(LightGreen, 0x40);
                        furi_hal_light_set(LightBlue, 0xff);
                        break;
                    case 8:
                        furi_hal_light_set(LightRed, 0x00);
                        furi_hal_light_set(LightGreen, 0x00);
                        furi_hal_light_set(LightBlue, 0xff);
                        break;
                    case 9:
                        furi_hal_light_set(LightRed, 0x40);
                        furi_hal_light_set(LightGreen, 0x00);
                        furi_hal_light_set(LightBlue, 0xff);
                        break;
                    case 10:
                        furi_hal_light_set(LightRed, 0xff);
                        furi_hal_light_set(LightGreen, 0x00);
                        furi_hal_light_set(LightBlue, 0xff);
                        break;
                    case 11:
                        furi_hal_light_set(LightRed, 0xff);
                        furi_hal_light_set(LightGreen, 0x40);
                        furi_hal_light_set(LightBlue, 0xff);
                        break;
                    }

                    furi_mutex_release(app->mutex);

                    if(!continue_playing) break;
                    executor_keychain_play_continuous_sound(app);
                    furi_delay_ms(50);
                }

                furi_hal_speaker_stop();
                furi_hal_speaker_release();
                speaker_acquired = false;

                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->is_playing = false;
                app->circle_size = 8;
                furi_hal_light_set(LightRed, 0x00);
                furi_hal_light_set(LightGreen, 0x00);
                furi_hal_light_set(LightBlue, 0x00);
                furi_mutex_release(app->mutex);
            }
        }
        furi_delay_ms(10);
    }

    if(speaker_acquired) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    return 0;
}

static void executor_keychain_input_callback(InputEvent* input_event, void* ctx) {
    ExecutorKeychainApp* app = ctx;

    if(input_event->type == InputTypePress || input_event->type == InputTypeRelease) {
        if(input_event->key == InputKeyOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->button_pressed = (input_event->type == InputTypePress);
            furi_mutex_release(app->mutex);
        }
    }

    if(input_event->type == InputTypeShort) {
        switch(input_event->key) {
        case InputKeyLeft:
            if(app->selected_button > 0) {
                app->selected_button--;
            }
            break;
        case InputKeyRight:
            if(app->selected_button < MAX_BUTTONS - 1) {
                app->selected_button++;
            }
            break;
        case InputKeyUp:
            if(app->selected_button >= 4) {
                app->selected_button -= 4;
            }
            break;
        case InputKeyDown:
            if(app->selected_button < 4) {
                app->selected_button += 4;
            }
            break;
        case InputKeyBack:
            app->running = false;
            break;
        default:
            break;
        }
    }
}

int32_t executor_keychain_app(void* p) {
    UNUSED(p);

    ExecutorKeychainApp* app = malloc(sizeof(ExecutorKeychainApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->selected_button = 0;
    app->is_playing = false;
    app->running = true;
    app->button_pressed = false;
    app->current_sound = 0;
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->led_color = 0;
    app->circle_size = 8;
    app->circle_growing = true;
    app->current_name = NAMES[furi_hal_random_get() % NUM_NAMES];
    app->sound_thread = furi_thread_alloc();
    furi_thread_set_name(app->sound_thread, "SoundThread");
    furi_thread_set_stack_size(app->sound_thread, 2048);
    furi_thread_set_context(app->sound_thread, app);
    furi_thread_set_callback(app->sound_thread, executor_keychain_sound_thread_callback);
    furi_thread_start(app->sound_thread);

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, executor_keychain_draw_callback, app);
    view_port_input_callback_set(view_port, executor_keychain_input_callback, app);
    view_port_set_orientation(view_port, ViewPortOrientationVertical);

    gui_add_view_port(app->gui, view_port, GuiLayerFullscreen);

    while(app->running) {
        view_port_update(view_port);
        furi_delay_ms(100);
    }
    furi_thread_join(app->sound_thread);
    furi_thread_free(app->sound_thread);
    furi_mutex_free(app->mutex);

    gui_remove_view_port(app->gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);

    return 0;
}
