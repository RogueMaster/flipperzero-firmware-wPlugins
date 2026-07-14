#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/elements.h>

#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include <flipper_format/flipper_format.h>

// some magic
#include "cigarette_icons.h"

typedef struct {
    int32_t x, y;
} ImagePosition;

static ImagePosition image_position = {.x = 12, .y = 30};
static ImagePosition smoke_position = {.x = 0, .y = 0};

typedef struct {
    int32_t counter, maxdrags;
    bool lit, done;
    uint8_t style; // 0 = regular, 1 = skinny
    IconAnimation* icon;
    NotificationApp* notification;
    uint32_t smoke_end; // Tracks when to stop smoke
    bool cough_used; // NEW - tracks if cough was used
} Cigarette;

static Cigarette cigarette = {.counter = 0, .maxdrags = 13, .lit = false, .done = false};

const NotificationMessage message_delay_15 = {
    .type = NotificationMessageTypeDelay,
    .data.delay.length = 15,
};

static const NotificationSequence sequence_puff = {
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};

static const NotificationSequence sequence_light_up = {
    &message_red_255,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_red_0,
    NULL,
};
static const NotificationSequence sequence_cough = {
    &message_red_255,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_red_0,
    &message_delay_50,
    &message_delay_50,
    &message_delay_50,
    &message_delay_50,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    &message_delay_50,
    &message_delay_50,
    &message_red_255,
    &message_delay_25,
    &message_red_0,

    NULL,
};

typedef struct {
    uint32_t total_smokes;
    bool visible;
    Storage* storage;
} StatsView;

typedef struct {
    bool visible;
    Storage* storage;
} AboutView;

static StatsView stats_view = {.total_smokes = 0, .visible = false};
static AboutView about_view = {.visible = false};

static void app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);

    if(stats_view.visible) {
        char buffer[64];
        // 1 cig in app = 15 IRL cigs (one pack)
        // 14.7% cancer risk after 20 years of 15/day
        float cancer_pct = (stats_view.total_smokes * 15.0f) / (15 * 365 * 20) * 14.7f;
        if(cancer_pct > 100.0f) cancer_pct = 100.0f;

        snprintf(buffer, sizeof(buffer), "darts smoked: %lu", stats_view.total_smokes);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, buffer);

        if(cancer_pct >= 100.0f) {
            canvas_draw_str_aligned(
                canvas, 64, 38, AlignCenter, AlignCenter, "YOU DIED OF CANCER");
            canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "RIP BOZO");
        } else {
            snprintf(buffer, sizeof(buffer), "%.1f%%", (double)cancer_pct);
            canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "cancer progress:");
            elements_progress_bar_with_text(canvas, 4, 52, 120, cancer_pct / 100, buffer);

            if(cancer_pct >= 50.0f && cancer_pct < 100.0f) {
                canvas_draw_str_aligned(
                    canvas, 64, 36, AlignCenter, AlignCenter, "SEEK HELP IMMEDIATELY");
            }
        }

        elements_button_up(canvas, "about");
        elements_button_down(canvas, "back");
        return;
    }

    if(about_view.visible) {
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "by maz");
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "special thanks to");
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "@jaylikesbunda");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "xoxo");

        elements_button_down(canvas, "back");
        return;
    }

    // Draw smoke only when smoke_end is active
    if(cigarette.smoke_end) {
        canvas_draw_icon_animation(canvas, smoke_position.x, smoke_position.y, cigarette.icon);
    } else if(cigarette.lit && !cigarette.done) {
        elements_button_right(canvas, "puff");
        canvas_draw_icon_animation(canvas, smoke_position.x, smoke_position.y, cigarette.icon);
    }

    if(cigarette.lit == false && cigarette.done == false) {
        elements_button_center(canvas, "light up");
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "< switch style >");
        elements_button_up(canvas, "stats");
    }

    if(cigarette.done == true) {
        elements_button_left(canvas, "another one");
    }

    //elements_button_up(canvas, "stats");

    // Cigarette icon selection based on style
    const Icon* current_icon = NULL;
    switch(cigarette.counter) {
    case 0:
        current_icon = (cigarette.style == 0) ? &I_cigarette0_115x25 : &I_skinny0_112x20;
        break;
    case 1:
        current_icon = (cigarette.style == 0) ? &I_cigarette1_115x25 : &I_skinny1_112x20;
        break;
    case 2:
        current_icon = (cigarette.style == 0) ? &I_cigarette2_115x25 : &I_skinny2_112x20;
        break;
    case 3:
        current_icon = (cigarette.style == 0) ? &I_cigarette3_115x25 : &I_skinny3_112x20;
        break;
    case 4:
        current_icon = (cigarette.style == 0) ? &I_cigarette4_115x25 : &I_skinny4_112x20;
        break;
    case 5:
        current_icon = (cigarette.style == 0) ? &I_cigarette5_115x25 : &I_skinny5_112x20;
        break;
    case 6:
        current_icon = (cigarette.style == 0) ? &I_cigarette6_115x25 : &I_skinny6_112x20;
        break;
    case 7:
        current_icon = (cigarette.style == 0) ? &I_cigarette7_115x25 : &I_skinny7_112x20;
        break;
    case 8:
        current_icon = (cigarette.style == 0) ? &I_cigarette8_115x25 : &I_skinny8_112x20;
        break;
    case 9:
        current_icon = (cigarette.style == 0) ? &I_cigarette9_115x25 : &I_skinny9_112x20;
        break;
    case 10:
        current_icon = (cigarette.style == 0) ? &I_cigarette10_115x25 : &I_skinny10_112x20;
        break;
    case 11:
        current_icon = (cigarette.style == 0) ? &I_cigarette11_115x25 : &I_skinny11_112x20;
        break;
    case 12:
        current_icon = (cigarette.style == 0) ? &I_cigarette12_115x25 : &I_skinny12_112x20;
        break;
    case 13:
        current_icon = (cigarette.style == 0) ? &I_cigarette13_115x25 : &I_skinny13_112x20;
        cigarette.done = true;
        break;
    default:
        current_icon = (cigarette.style == 0) ? &I_cigarette13_115x25 : &I_skinny13_112x20;
        break;
    }
    canvas_draw_icon(canvas, image_position.x, image_position.y, current_icon);
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);

    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t cigarette_main(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    cigarette.icon = icon_animation_alloc(&A_cigarette_smoke);

    // Configure view port
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, app_draw_callback, NULL);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);

    // Register view port in GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    cigarette.notification = furi_record_open(RECORD_NOTIFICATION);

    stats_view.storage = furi_record_open(RECORD_STORAGE);

    // Load stats
    FlipperFormat* ff_load = flipper_format_file_alloc(stats_view.storage);
    storage_common_mkdir(stats_view.storage, "/ext/apps_data/cigarette");
    if(flipper_format_file_open_existing(ff_load, "/ext/apps_data/cigarette/stats.txt")) {
        flipper_format_read_uint32(ff_load, "total", &stats_view.total_smokes, 1);
    }
    flipper_format_free(ff_load);

    InputEvent event;

    bool running = true;
    while(running) {
        // Check if smoke needs to stop
        if(cigarette.smoke_end && furi_get_tick() >= cigarette.smoke_end) {
            icon_animation_stop(cigarette.icon);
            cigarette.smoke_end = 0;
        }

        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if((event.type == InputTypePress) || (event.type == InputTypeRepeat)) {
                switch(event.key) {
                case InputKeyLeft:
                    if(!stats_view.visible && !cigarette.lit && !cigarette.done) {
                        // Cycle through styles
                        cigarette.style = (cigarette.style - 1 + 2) % 2;
                    } else if(cigarette.done) {
                        // Reset positions when getting new cig
                        stats_view.total_smokes++;
                        cigarette.counter = 0;
                        cigarette.lit = false;
                        cigarette.done = false;
                        cigarette.cough_used = false; // RESET FLAG
                        smoke_position.x = 0; // Reset smoke position
                        image_position.x = 12; // Reset cig position
                        icon_animation_stop(cigarette.icon);
                    }
                    break;
                case InputKeyRight:
                    if(!stats_view.visible && !cigarette.lit && !cigarette.done) {
                        cigarette.style = (cigarette.style + 1) % 2;
                    } else if(cigarette.lit) {
                        if(!cigarette.done) {
                            cigarette.counter++;
                            if(cigarette.counter > cigarette.maxdrags) {
                                // Secret cough with 1s smoke
                                icon_animation_start(cigarette.icon);
                                cigarette.smoke_end = furi_get_tick() + 1000;
                                notification_message(cigarette.notification, &sequence_cough);
                                cigarette.counter = cigarette.maxdrags;
                            } else {
                                if(cigarette.counter == cigarette.maxdrags) {
                                    cigarette.done = true;
                                }
                                notification_message(cigarette.notification, &sequence_puff);
                                smoke_position.x += 5;
                                image_position.x += 5;
                            }
                        } else {
                            if(!cigarette.cough_used) { // CHECK FLAG
                                // Cough puff with reset smoke position
                                smoke_position.x = image_position.x - 3; // Start from cig tip
                                icon_animation_start(cigarette.icon);
                                cigarette.smoke_end = furi_get_tick() + 1000;
                                notification_message(cigarette.notification, &sequence_cough);
                                cigarette.cough_used = true; // SET FLAG
                            }
                        }
                    }
                    break;
                case InputKeyUp:
                    if(event.type == InputTypePress) {
                        //stats_view.visible = !stats_view.visible;
                        if(!stats_view.visible) {
                            stats_view.visible = true;
                        } else {
                            about_view.visible = true;
                            stats_view.visible = false;
                        }
                    }
                    break;
                case InputKeyDown:
                    if(stats_view.visible) {
                        stats_view.visible = false;
                        //about_view.visible = true;
                    } else if(about_view.visible) {
                        stats_view.visible = true;
                        about_view.visible = false;
                    }
                    break;
                case InputKeyOk:
                    if(cigarette.counter == 0 && !cigarette.lit) {
                        cigarette.lit = true;
                        notification_message(cigarette.notification, &sequence_light_up);
                        icon_animation_start(cigarette.icon);
                        notification_message(cigarette.notification, &sequence_blink_red_10);
                    }
                    break;
                default:
                    running = false;
                    break;
                }
            }
        }
        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);

    // Save stats before exit
    FlipperFormat* ff_save = flipper_format_file_alloc(stats_view.storage);
    if(flipper_format_file_open_always(ff_save, "/ext/apps_data/cigarette/stats.txt")) {
        flipper_format_write_header_cstr(ff_save, "CigaretteStats", 1);
        flipper_format_write_uint32(ff_save, "total", &stats_view.total_smokes, 1);
    }
    flipper_format_free(ff_save);
    furi_record_close(RECORD_STORAGE);

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);

    furi_record_close(RECORD_GUI);

    return 0;
}
