#include <furi.h>
#include <dialogs/dialogs.h>
#include <stream/file_stream.h>
#include <gui/gui.h>

#include <image_viewer_icons.h>

constexpr auto tag = "Image viewer";

constexpr uint32_t msg_count = 8UL;

constexpr uint32_t timeout = 100UL;

Stream* stream;

static void draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    const size_t width = canvas_width(canvas);
    const size_t height = canvas_height(canvas);

    const size_t image_size = ((width + 7) / 8) * height;
    const size_t file_size = stream_size(stream);
    canvas_clear(canvas);

    // Accept raw pixels and the uncompressed Flipper bitmap header used by
    // the bundled .bm files. Never pass a raw buffer to the compressed API.
    bool valid = (file_size == image_size || file_size == image_size + 1) &&
                 stream_rewind(stream);
    if(valid && file_size == image_size + 1) {
        uint8_t header = 0;
        valid = stream_read(stream, &header, 1) == 1 && header == 0;
    }
    if(!valid) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 32, "Invalid image format");
        return;
    }

    uint8_t* image_data = new uint8_t[image_size];
    if(stream_read(stream, image_data, image_size) == image_size) {
        canvas_draw_xbm(canvas, 0, 0, width, height, image_data);
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 32, "Cannot read image");
    }

    delete[] image_data;
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);

    FuriMessageQueue* event_queue = static_cast<FuriMessageQueue*>(ctx);
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

extern "C" {
int32_t image_viewer_main(void* p) {
    UNUSED(p);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(msg_count, sizeof(InputEvent));

    Storage* storage = static_cast<Storage*>(furi_record_open(RECORD_STORAGE));

    stream = file_stream_alloc(storage);

    furi_record_close(RECORD_STORAGE);

    DialogsApp* dialogs = static_cast<DialogsApp*>(furi_record_open(RECORD_DIALOGS));

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".bm", &I_icon);
    browser_options.hide_ext = false;

    FuriString* path = furi_string_alloc_set_str("/ext/apps_assets/image_viewer");

    bool selected = dialog_file_browser_show(dialogs, path, path, &browser_options);

    furi_record_close(RECORD_DIALOGS);

    if(selected) {
        if(!file_stream_open(stream, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E(tag, "Cannot open file \"%s\"", furi_string_get_cstr(path));
            return -1;
        }
    }

    furi_string_free(path);

    ViewPort* view_port = view_port_alloc();

    view_port_draw_callback_set(view_port, draw_callback, nullptr);

    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = static_cast<Gui*>(furi_record_open(RECORD_GUI));

    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;

    while(true) {
        if(furi_message_queue_get(event_queue, &event, timeout) == FuriStatusOk &&
           event.key == InputKeyBack) {
            break;
        }
        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);

    gui_remove_view_port(gui, view_port);

    view_port_free(view_port);

    furi_message_queue_free(event_queue);

    furi_record_close(RECORD_GUI);

    stream_free(stream);

    return 0;
}
}
