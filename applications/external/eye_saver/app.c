#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define EYE_TIMER_MS      (20 * 60 * 1000) // 20 minutes
#define BREAK_TIME_MS     (20 * 1000) // 20 seconds
#define ABOUT_TEXT_HEIGHT 80
#define SCREEN_HEIGHT     64

typedef enum {
    SCREEN_TIMER,
    SCREEN_BREAK,
    SCREEN_ABOUT,
} AppScreen;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    NotificationApp* notifications;
    AppScreen screen;
    uint32_t timer_start_time;
    uint32_t break_start_time;
    bool exit;
    int about_scroll_offset;
} EyeSaverApp;

uint32_t get_elapsed_ms(uint32_t start) {
    return furi_get_tick() * 1000 / furi_kernel_get_tick_frequency() - start;
}

void draw_timer(Canvas* canvas, void* ctx) {
    EyeSaverApp* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    uint32_t elapsed = get_elapsed_ms(app->timer_start_time);
    uint32_t remaining_ms = (elapsed >= EYE_TIMER_MS) ? 0 : EYE_TIMER_MS - elapsed;
    uint32_t minutes = remaining_ms / 60000;
    uint32_t seconds = (remaining_ms % 60000) / 1000;

    char time_buf[32];
    snprintf(
        time_buf,
        sizeof(time_buf),
        "Next break in: %02lu:%02lu",
        (unsigned long)minutes,
        (unsigned long)seconds);
    canvas_draw_str(canvas, 5, 18, time_buf);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 62, "OK for About, Back to exit");
}

void draw_break(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 5, 20, "Take a break!");
    canvas_draw_str(canvas, 5, 35, "Look outside 20 sec");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 62, "Back to exit");
}

void draw_about(Canvas* canvas, void* ctx) {
    EyeSaverApp* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    const int screen_height = 64;
    const int footer_height = 12;
    const int scroll_area_height = screen_height - footer_height;

    int y = 15 - app->about_scroll_offset;
    if(y + 12 > 0 && y < scroll_area_height) canvas_draw_str(canvas, 5, y, "Eye Saver App");
    y += 12;
    if(y + 12 > 0 && y < scroll_area_height) canvas_draw_str(canvas, 5, y, "Reminds you to take");
    y += 12;
    if(y + 12 > 0 && y < scroll_area_height) canvas_draw_str(canvas, 5, y, "a 20 sec break every");
    y += 12;
    if(y + 12 > 0 && y < scroll_area_height) canvas_draw_str(canvas, 5, y, "20 minutes.");
    y += 12;
    if(y + 12 > 0 && y < scroll_area_height) canvas_draw_str(canvas, 5, y, "Author: @paul-sopin");
    y += 12;
    if(y + 12 > 0 && y < scroll_area_height)
        canvas_draw_str(canvas, 5, y, "github.com/paul-sopin");

    canvas_draw_str(canvas, 5, screen_height - footer_height + 12, "Back to timer");

    int bar_x = 124;
    int bar_y = 0;
    int bar_width = 4;
    int bar_height = scroll_area_height;

    canvas_draw_line(canvas, bar_x, bar_y, bar_x + bar_width - 1, bar_y);
    canvas_draw_line(
        canvas, bar_x, bar_y + bar_height - 1, bar_x + bar_width - 1, bar_y + bar_height - 1);
    canvas_draw_line(canvas, bar_x, bar_y, bar_x, bar_y + bar_height - 1);
    canvas_draw_line(
        canvas, bar_x + bar_width - 1, bar_y, bar_x + bar_width - 1, bar_y + bar_height - 1);

    int content_height = ABOUT_TEXT_HEIGHT;
    int visible_height = scroll_area_height;
    int max_scroll = content_height - visible_height;
    if(max_scroll < 0) max_scroll = 0;

    int thumb_height = (visible_height * visible_height) / content_height;
    if(thumb_height < 5) thumb_height = 5;

    int thumb_y = (app->about_scroll_offset * (visible_height - thumb_height)) / max_scroll;

    for(int i = 0; i < thumb_height; i++) {
        canvas_draw_line(
            canvas, bar_x + 1, bar_y + thumb_y + i, bar_x + bar_width - 2, bar_y + thumb_y + i);
    }
}

void input_callback(InputEvent* event, void* ctx) {
    EyeSaverApp* app = ctx;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            if(app->screen == SCREEN_ABOUT) {
                app->about_scroll_offset = 0;
                app->screen = SCREEN_TIMER;
                view_port_draw_callback_set(app->view_port, draw_timer, app);
            } else {
                app->exit = true;
            }
        } else if(event->key == InputKeyOk && app->screen == SCREEN_TIMER) {
            app->about_scroll_offset = 0;
            app->screen = SCREEN_ABOUT;
            view_port_draw_callback_set(app->view_port, draw_about, app);
        } else if(app->screen == SCREEN_ABOUT) {
            if(event->key == InputKeyUp) {
                if(app->about_scroll_offset > 0) {
                    app->about_scroll_offset -= 12;
                    if(app->about_scroll_offset < 0) app->about_scroll_offset = 0;
                }
                view_port_update(app->view_port);
            } else if(event->key == InputKeyDown) {
                int max_scroll = ABOUT_TEXT_HEIGHT - SCREEN_HEIGHT + 12;
                if(app->about_scroll_offset < max_scroll) {
                    app->about_scroll_offset += 12;
                    if(app->about_scroll_offset > max_scroll)
                        app->about_scroll_offset = max_scroll;
                }
                view_port_update(app->view_port);
            }
        }
    }

    view_port_update(app->view_port);
}

int32_t main_eye_saver_app(void* p) {
    UNUSED(p);

    EyeSaverApp* app = malloc(sizeof(EyeSaverApp));
    app->exit = false;
    app->screen = SCREEN_TIMER;
    app->timer_start_time = furi_get_tick() * 1000 / furi_kernel_get_tick_frequency();
    app->break_start_time = 0;
    app->about_scroll_offset = 0;

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_timer, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    while(!app->exit) {
        if(app->screen == SCREEN_TIMER) {
            uint32_t elapsed = get_elapsed_ms(app->timer_start_time);
            if(elapsed >= EYE_TIMER_MS) {
                notification_message(app->notifications, &sequence_double_vibro);
                app->screen = SCREEN_BREAK;
                app->break_start_time = furi_get_tick() * 1000 / furi_kernel_get_tick_frequency();
                view_port_draw_callback_set(app->view_port, draw_break, app);
            }
        } else if(app->screen == SCREEN_BREAK) {
            uint32_t break_elapsed = get_elapsed_ms(app->break_start_time);
            if(break_elapsed >= BREAK_TIME_MS) {
                app->timer_start_time = furi_get_tick() * 1000 / furi_kernel_get_tick_frequency();
                app->screen = SCREEN_TIMER;
                view_port_draw_callback_set(app->view_port, draw_timer, app);
            }
        }

        view_port_update(app->view_port);
        furi_delay_ms(100);
    }

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);

    return 0;
}
