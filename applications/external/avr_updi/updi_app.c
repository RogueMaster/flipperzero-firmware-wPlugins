#include "updi_app_i.h"

#include "scenes/updi_scene.h"

#include <storage/storage.h>

#include <stdio.h>
#include <string.h>

const char* updi_pins_label(UpdiPins pins) {
    return pins == UpdiPins13_14 ? "13/14" : "15/16";
}

void updi_pins_load(UpdiApp* app) {
    app->pins = UpdiPins15_16;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, UPDI_PINS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[8];
        memset(buf, 0, sizeof(buf));
        storage_file_read(file, buf, sizeof(buf) - 1);
        if(buf[0] == '1' && buf[1] == '3') app->pins = UpdiPins13_14;
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void updi_pins_save(UpdiApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, UPDI_APPS_DATA_DIR);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, UPDI_PINS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* text = app->pins == UpdiPins13_14 ? "13\n" : "15\n";
        storage_file_write(file, text, strlen(text));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void updi_progress_draw(Canvas* canvas, void* model) {
    UpdiProgressModel* progress = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "AVR UPDI");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 28, progress->status);

    canvas_draw_frame(canvas, 2, 40, 124, 12);
    uint8_t percent = progress->percent > 100 ? 100 : progress->percent;
    canvas_draw_box(canvas, 4, 42, (uint8_t)((120u * percent) / 100u), 8);

    char label[8];
    snprintf(label, sizeof(label), "%u%%", percent);
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, label);
}

static bool updi_custom_event(void* context, uint32_t event) {
    UpdiApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool updi_navigation_event(void* context) {
    UpdiApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static UpdiApp* updi_app_alloc(void) {
    UpdiApp* app = malloc(sizeof(UpdiApp));
    furi_check(app);
    memset(app, 0, sizeof(UpdiApp));

    app->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(app->expansion);

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->hex_path = furi_string_alloc();
    app->eeprom_path = furi_string_alloc();
    snprintf(app->dump_name, sizeof(app->dump_name), "dump");
    updi_pins_load(app);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->scene_manager = scene_manager_alloc(&updi_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, updi_custom_event);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, updi_navigation_event);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, UpdiViewSubmenu, submenu_get_view(app->submenu));

    app->var_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, UpdiViewVarList, variable_item_list_get_view(app->var_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, UpdiViewWidget, widget_get_view(app->widget));

    app->progress = view_alloc();
    view_allocate_model(app->progress, ViewModelTypeLocking, sizeof(UpdiProgressModel));
    view_set_draw_callback(app->progress, updi_progress_draw);
    view_dispatcher_add_view(app->view_dispatcher, UpdiViewProgress, app->progress);

    app->text_box = text_box_alloc();
    text_box_set_font(app->text_box, TextBoxFontText);
    view_dispatcher_add_view(app->view_dispatcher, UpdiViewText, text_box_get_view(app->text_box));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, UpdiViewTextInput, text_input_get_view(app->text_input));

    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, UpdiSceneMenu);
    return app;
}

static void updi_app_free(UpdiApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, UpdiViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, UpdiViewVarList);
    view_dispatcher_remove_view(app->view_dispatcher, UpdiViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, UpdiViewProgress);
    view_dispatcher_remove_view(app->view_dispatcher, UpdiViewText);
    view_dispatcher_remove_view(app->view_dispatcher, UpdiViewTextInput);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_list);
    widget_free(app->widget);
    view_free(app->progress);
    text_box_free(app->text_box);
    text_input_free(app->text_input);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);
    furi_string_free(app->hex_path);
    furi_string_free(app->eeprom_path);
    furi_mutex_free(app->mutex);

    expansion_enable(app->expansion);
    furi_record_close(RECORD_EXPANSION);
    free(app);
}

int32_t avr_updi_app(void* arg) {
    UNUSED(arg);
    UpdiApp* app = updi_app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    updi_app_free(app);
    return 0;
}
