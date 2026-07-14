#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>

#include "banana.h"
#include "banana_icons.h"

const Menu menu[] = {
    {START, "Start"},
    {ABOUT, "About"},
    {EXIT, "Exit"},
    {0, NULL},
};

const char* about[] = {
    "Banana is love",
    "Banana is life",
    "",
    "@DrEverr, 2024",
    "Press any key to return",
    0,
};

uint8_t menu_size(const Menu* menu) {
    uint8_t size = 0;
    while(menu[size].name != 0)
        size++;
    return size;
}

void save_state(GameState* state) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));

    if(storage_file_open(file, APP_DATA_PATH("save.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, state, sizeof(GameState));
    }

    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);
}

void load_state(GameState* state) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));

    if(storage_file_open(file, APP_DATA_PATH("save.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, state, sizeof(GameState));
    }

    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);
}

void draw_banana(Canvas* canvas, GameState* state) {
    furi_assert(state);

    canvas_clear(canvas);

    // Draw banana image
    if(state->inverted) {
        canvas_draw_icon(canvas, 32, 0, &I_banana_black_64_64);
    } else {
        canvas_draw_icon(canvas, 32, 0, &I_banana_64_64);
    }

    // Draw counter
    char counter_str[16];
    snprintf(counter_str, sizeof(counter_str), "%lu", state->counter);
    canvas_draw_str_aligned(canvas, 64, 64, AlignCenter, AlignBottom, counter_str);
}

void draw_menu(Canvas* canvas, const Menu* menu, uint8_t selected) {
    uint8_t size = menu_size(menu);
    canvas_set_font(canvas, FontSecondary);
    int item_height = canvas_current_font_height(canvas);
    int item_max_width = 0;
    for(uint8_t i = 0; i < size; i++) {
        int item_width = canvas_string_width(canvas, menu[i].name);
        if(item_width > item_max_width) {
            item_max_width = item_width;
        }
    }
    item_max_width += MARGIN;
    for(uint8_t i = 0; i < size; i++) {
        canvas_set_color(canvas, ColorBlack);
        if(i == selected) {
            canvas_draw_box(
                canvas,
                SCREEN_WIDTH / 2 - item_max_width / 2,
                SCREEN_HEIGHT / 2 - item_height * size / 2 + i * item_height + MARGIN,
                item_max_width + MARGIN * 2,
                item_height);
        }
        canvas_set_color(canvas, i == selected ? ColorWhite : ColorBlack);
        canvas_draw_str_aligned(
            canvas,
            SCREEN_WIDTH / 2 + MARGIN,
            SCREEN_HEIGHT / 2 - item_height * size / 2 + i * item_height + item_height / 2 +
                MARGIN,
            AlignCenter,
            AlignCenter,
            menu[i].name);
    }
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(
        canvas,
        SCREEN_WIDTH / 2 - item_max_width / 2,
        SCREEN_HEIGHT / 2 - item_height * size / 2,
        item_max_width + MARGIN * 2,
        item_height * size + MARGIN * 2,
        2);
}

void draw_about(Canvas* canvas) {
    uint8_t size = 0;
    while(about[size] != 0)
        size++;

    canvas_set_font(canvas, FontSecondary);
    int item_height = canvas_current_font_height(canvas);
    int item_max_width = 0;
    for(uint8_t i = 0; i < size; i++) {
        int item_width = canvas_string_width(canvas, about[i]);
        if(item_width > item_max_width) {
            item_max_width = item_width;
        }
    }
    canvas_set_color(canvas, ColorBlack);
    for(uint8_t i = 0; i < size; i++) {
        canvas_set_font(canvas, i == 0 ? FontPrimary : FontSecondary);
        canvas_draw_str_aligned(
            canvas,
            SCREEN_WIDTH / 2 - item_max_width / 2,
            SCREEN_HEIGHT / 2 - item_height * size / 2 + i * item_height + item_height / 2,
            AlignLeft,
            AlignCenter,
            about[i]);
    }
}

void draw_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    AppState* app_state = ctx;

    furi_mutex_acquire(app_state->mutex, FuriWaitForever);

    canvas_clear(canvas);
    if(app_state->app_status == MENU) {
        draw_menu(canvas, menu, app_state->menu_selected);
    } else if(app_state->app_status == INFO) {
        draw_about(canvas);
    } else if(app_state->app_status == PLAYING) {
        draw_banana(canvas, app_state->game_state);
    }

    furi_mutex_release(app_state->mutex);
}

void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);

    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t banana_main(void* p) {
    UNUSED(p);

    AppState* app_state = malloc(sizeof(AppState));
    app_state->app_status = MENU;
    app_state->menu_selected = 0;
    app_state->game_state = malloc(sizeof(GameState));
    app_state->game_state->counter = 0;
    app_state->game_state->inverted = false;

    app_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    load_state(app_state->game_state);

    InputEvent event;
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, app_state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    bool running = true;
    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if((event.type != InputTypeShort) && (event.type != InputTypeLong)) continue;

            furi_mutex_acquire(app_state->mutex, FuriWaitForever);

            if(app_state->app_status == MENU) {
                switch(event.key) {
                case InputKeyUp:
                    if(app_state->menu_selected > 0) {
                        app_state->menu_selected--;
                    }
                    break;
                case InputKeyDown:
                    if(app_state->menu_selected < menu_size(menu)) {
                        app_state->menu_selected++;
                    }
                    break;
                case InputKeyOk:
                    switch(menu[app_state->menu_selected].id) {
                    case START:
                        app_state->app_status = PLAYING;
                        break;
                    case ABOUT:
                        app_state->app_status = INFO;
                        break;
                    case EXIT:
                        running = false;
                        break;
                    default:
                        break;
                    }
                    break;
                case InputKeyBack:
                    running = false;
                    break;
                default:
                    break;
                }
            } else if(app_state->app_status == INFO) {
                app_state->app_status = MENU;
                app_state->menu_selected = 0;
            } else if(app_state->app_status == PLAYING) {
                switch(event.key) {
                case InputKeyOk:
                    app_state->game_state->counter++;
                    app_state->game_state->inverted = true;
                    break;
                case InputKeyBack:
                    save_state(app_state->game_state);
                    app_state->app_status = MENU;
                    app_state->menu_selected = 0;
                    break;
                default:
                    break;
                }
            }

            furi_mutex_release(app_state->mutex);
        }

        view_port_update(view_port);
        if(app_state->game_state->inverted) {
            furi_hal_vibro_on(true);
            furi_delay_ms(100);
            furi_hal_vibro_on(false);
            app_state->game_state->inverted = false;
        }
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    free(app_state->game_state);
    furi_mutex_free(app_state->mutex);
    free(app_state);

    furi_record_close(RECORD_GUI);

    return 0;
}
