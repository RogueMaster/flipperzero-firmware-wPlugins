#include "../../app.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <postmanflipx_icons.h>
#include <compress.h>

typedef struct {
    size_t progress;
    bool done;
    FuriString* uart_message;
} DownloadProgressModel;

static void bytes_to_human_readable(size_t bytes, FuriString* output) {
    const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    size_t suffix_index = 0;
    double readable_size = (double)bytes;

    while(readable_size >= 1024 && suffix_index < (sizeof(suffixes) / sizeof(suffixes[0])) - 1) {
        readable_size /= 1024;
        suffix_index++;
    }

    furi_string_printf(output, "%.2f %s", readable_size, suffixes[suffix_index]);
}

#define TOTAL_FRAMES    10 // Used for animating the flipper through download
#define BYTES_PER_FRAME 2096 // Change frame per RX_BUF size

// SD CARD
static const uint8_t image_SDcardMounted_0_bits[] =
    {0xff, 0xff, 0xff, 0xfe, 0xfe, 0xff, 0xff, 0xfe, 0xaa, 0xaa, 0xfe};

// Folder down icon
static const uint8_t image_arrow_curved_down_right_0_bits[] =
    {0x04, 0x02, 0x02, 0x03, 0x17, 0x1e, 0x1c, 0x1e};

// Flipper action button
static const uint8_t image_ButtonCenter_0_bits[] = {0x1c, 0x22, 0x5d, 0x5d, 0x5d, 0x22, 0x1c};

static void download_progress_draw_callback(Canvas* canvas, void* _model) {
    DownloadProgressModel* model = _model;
    furi_assert(model != NULL);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    if(!model->done) {
        FuriString* progress_text = furi_string_alloc();

        uint32_t current_frame =
            (model->progress / BYTES_PER_FRAME) % icon_get_frame_count(&A_Download_128);

        FURI_LOG_D(TAG, "Current frame: %lu", current_frame);

        // Get the frame data for the current frame
        const uint8_t* compressed_frame_data = icon_get_frame_data(&A_Download_128, current_frame);
        // Draw the current frame of the animated icon
        canvas_draw_bitmap(
            canvas,
            0,
            0,
            icon_get_width(&A_Download_128),
            icon_get_height(&A_Download_128),
            compressed_frame_data);

        bytes_to_human_readable(model->progress, progress_text);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 78, 45, furi_string_get_cstr(progress_text));
        furi_string_free(progress_text);
        canvas_draw_str(canvas, 64, 58, "streaming ...");
    }

    if(model->done) {
        bool no_error = !strstr(furi_string_get_cstr(model->uart_message), "DOWNLOAD_ERROR:");
        if(model->uart_message != NULL && no_error) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 2, 13, "Success !");
            canvas_draw_xbm(canvas, 2, 20, 8, 11, image_SDcardMounted_0_bits);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 14, 30, "/apps_data");
            canvas_draw_xbm(canvas, 3, 36, 5, 8, image_arrow_curved_down_right_0_bits);
            canvas_draw_str(canvas, 14, 44, "/postmanflipx");
            canvas_draw_xbm(canvas, 3, 50, 5, 8, image_arrow_curved_down_right_0_bits);
            canvas_draw_str(canvas, 12, 58, furi_string_get_cstr(model->uart_message));

            canvas_draw_icon(canvas, 77, 16, &I_DolphinSaved_51x48);

        } else {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 2, 11, "Download Failed !");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, 2, furi_string_get_cstr(model->uart_message));
            canvas_draw_xbm(canvas, 3, 53, 7, 7, image_ButtonCenter_0_bits);
            canvas_draw_str(canvas, 14, 60, "Go back");
        }
    }
}

static bool download_progress_input_callback(InputEvent* event, void* context) {
    App* app = context;
    UNUSED(app);
    UNUSED(event);
    if(event->key == InputKeyBack || event->key == InputKeyOk) {
        switch(app->download_mode) {
        case DOWNLOAD_GET:
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, Get);
            return true;
        case DOWNLOAD_POST:
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, Post);
            return true;
        default:
            break;
        }
    }
    return false;
}

void update_download_progress(App* app, size_t progress, bool done) {
    with_view_model(
        app->view,
        DownloadProgressModel * model,
        {
            model->progress = progress;
            model->done = done;
            model->uart_message = app->text_box_store;
        },
        true);
}

void scene_on_enter_download_progress(void* context) {
    FURI_LOG_T(TAG, "Displaying DOWNLOAD view");
    App* app = context;
    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(DownloadProgressModel));
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, download_progress_draw_callback);
    view_set_input_callback(app->view, download_progress_input_callback);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_Download);
    update_download_progress(app, 0, false);

    bool success_download = false;
    switch(app->download_mode) {
    case DOWNLOAD_GET:
        success_download = saveToFileCommand(app->uart, app->get_state->url);
        break;
    case DOWNLOAD_POST:
        success_download =
            savePostToFileCommand(app->uart, app->post_state->url, app->post_state->payload);
    default:
        break;
    }

    if(success_download) {
        FURI_LOG_I(TAG, "Download success");

    } else {
        FURI_LOG_E(TAG, "Download failed");
        // Copy error message to the text_box_store
    }
}

void scene_on_exit_download_progress(void* context) {
    App* app = context;
    app->download_mode = DOWNLOAD_NONE;
    view_free_model(app->view);
}

bool scene_on_event_download_progress(void* context, SceneManagerEvent event) {
    App* app = context;
    furi_assert(app);
    furi_assert(event);
    bool consumed = false;
    switch(event.type) {
    case(SceneManagerEventTypeBack):
        switch(app->download_mode) {
        case DOWNLOAD_GET:
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, Get);
            consumed = false;
            break;
        case DOWNLOAD_POST:
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, Post);
            consumed = false;
        default:
            break;
        }
        break;
    default:
        consumed = false;
        break;
    }

    return consumed;
}
